"""Private reports, execution stages, and pytest reporting hooks."""
from contextlib import redirect_stdout, redirect_stderr
from contextvars import ContextVar
from datetime import datetime, timezone
from html import escape
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from time import perf_counter
import uuid
import xml.etree.ElementTree as ET
import pytest
from .core import component_of, level_of
from .configuration import get_settings


RUN_ENV = "TC_RUN_DIR"
CONSOLE_ENV = "TC_CONSOLE"
WORK_ENV = "TC_WORK_DIR"
XML_FILENAME = "result.xml"


def utcnow():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def run_folder_timestamp(value):
    """Readable UTC folder timestamp with English month and 12-hour AM/PM time."""
    instant = datetime.fromisoformat(value).astimezone(timezone.utc)
    month = ("Jan", "Feb", "Mar", "Apr", "May", "Jun",
             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec")[instant.month - 1]
    period = "AM" if instant.hour < 12 else "PM"
    return (instant.strftime("%d-") + month
            + instant.strftime("-%Y_%I-%M-%S_") + period + "_UTC")


def readable_timestamp(value):
    """Format an ISO timestamp for display, with an explicit UTC timezone."""
    instant = datetime.fromisoformat(value).astimezone(timezone.utc)
    return instant.strftime("%Y-%m-%d %H:%M:%S.") + f"{instant.microsecond // 1000:03d} UTC"


def redact(text):
    text = str(text)
    text = re.sub('(?i)("(?:password|passwd|token|secret|authorization)"\\s*:\\s*)"(?:\\\\.|[^"\\\\])*"', '\\1"[REDACTED]"', text)
    text = re.sub(r"(?i)(password|passwd|token|secret|authorization)([\"\x27]?\s*[:=]\s*)([^\s,;]+)",
                  r"\1\2[REDACTED]", text)
    return re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", text)


def sanitized(value):
    if isinstance(value, dict):
        return {str(key): ("[REDACTED]" if re.search(r"(?i)password|passwd|token|secret|authorization", str(key)) else sanitized(item)) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [sanitized(item) for item in value]
    return redact(value) if isinstance(value, str) else value


def private_write(path, text):
    path = Path(path)
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w", encoding="utf-8") as stream:
        stream.write(text)
    path.chmod(0o600)


def append_console(directory, title, text):
    """Append private, redacted diagnostics without terminal output."""
    if not text:
        return
    path = Path(directory) / "console.log"
    with os.fdopen(os.open(path, os.O_WRONLY | os.O_APPEND | os.O_CREAT, 0o600),
                   "w", encoding="utf-8") as stream:
        stream.write(redact("\n--- " + title + " ---\n" + str(text) + "\n"))


class Tee:
    def __init__(self, terminal, logfile):
        self.terminal, self.logfile = terminal, logfile

    def write(self, text):
        text = redact(text)
        self.terminal.write(text)
        self.logfile.write(text)
        self.flush()
        return len(text)

    def flush(self):
        self.terminal.flush()
        self.logfile.flush()

    def isatty(self):
        return False


class RunReport:
    def __init__(self, root, components, mode, selection, collection=False):
        self.started = utcnow()
        label = "-".join(components)
        stamp = run_folder_timestamp(self.started)
        root = Path(root)
        root.mkdir(parents=True, exist_ok=True, mode=0o700)
        if root.is_symlink():
            raise RuntimeError("Log root must not be a symlink")
        self.path = root / (stamp + "_" + label + "_" + uuid.uuid4().hex[:8])
        self.path.mkdir(mode=0o700)
        self.workspace = tempfile.TemporaryDirectory(prefix="tc-run-")
        self.work_path = Path(self.workspace.name)
        settings = get_settings()
        self.data = dict(producer=settings.producer, disclaimer=settings.disclaimer, run_id=self.path.name, started=self.started, components=list(components),
                         mode=mode, selection=selection, collection=collection, cases=[],
                         collected=0, deselected=0, collection_errors=[], backend=[])

    def console(self):
        return os.fdopen(os.open(self.path / "console.log", os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_APPEND, 0o600), "w", encoding="utf-8")

    def capture(self, logfile):
        return redirect_stdout(Tee(sys.stdout, logfile)), redirect_stderr(Tee(sys.stderr, logfile))

    def finish(self, exitcode, error=None):
        event_file = self.work_path / "events.json"
        if event_file.exists():
            try:
                events = json.loads(event_file.read_text(encoding="utf-8"))
                if not isinstance(events, dict) or not isinstance(events.get("cases"), list):
                    raise ValueError("Invalid event data")
                self.data.update(events)
            except (ValueError, OSError):
                error = error or "Could not read pytest reporting events"
        xml = self.path / XML_FILENAME
        tree = None
        if xml.exists():
            try:
                tree = ET.parse(xml)
            except (ET.ParseError, OSError):
                error = error or "Pytest XML report was incomplete or invalid"
                xml.unlink(missing_ok=True)
        data = self.data = sanitized(self.data)
        data.update(finished=utcnow(), exitcode=exitcode, run_error=redact(error or ""))
        data["duration"] = round((datetime.fromisoformat(data["finished"]) - datetime.fromisoformat(self.started)).total_seconds(), 3)
        counts = {name: 0 for name in ("passed", "failed", "error", "skipped", "not_run")}
        for case in data["cases"]:
            counts[case.get("outcome") if case.get("outcome") in counts else "not_run"] += 1
        data["counts"] = counts
        if exitcode == 130:
            status = "INTERRUPTED"
        elif error or data["collection_errors"]:
            status = "RUN ERROR"
        elif exitcode == 2:
            status = "INTERRUPTED"
        elif exitcode not in (0, 1, 5):
            status = "RUN ERROR"
        elif data["collection"]:
            status = "COLLECTION ONLY"
        elif counts["failed"] or counts["error"] or exitcode == 1:
            status = "FAILED"
        elif not counts["passed"]:
            status = "NO TESTS EXECUTED"
        elif counts["not_run"]:
            status = "INCOMPLETE"
        else:
            status = "PASSED WITH SKIPS" if counts["skipped"] else "PASSED"
        data["status"] = status
        xml = self.path / XML_FILENAME
        if not xml.exists():
            suite = ET.Element("testsuite", name="run-lifecycle", tests="0", failures="0", errors="0", skipped="0")
            if error or exitcode not in (0, 5):
                suite.set("tests", "1"); suite.set("errors", "1")
                case = ET.SubElement(suite, "testcase", name="run_initialization", classname="runner")
                ET.SubElement(case, "error", type="RunError").text = redact(error or "Pytest did not finish its XML report")
            ET.SubElement(suite, "system-out").text = "Run status: " + status + "; no successful test result is implied."
            private_write(xml, ET.tostring(suite, encoding="unicode"))
        else:
            for element in tree.iter():
                element.attrib = {key: redact(value) for key, value in element.attrib.items()}
                if element.text:
                    element.text = redact(element.text)
                if element.tail:
                    element.tail = redact(element.tail)
            private_write(xml, ET.tostring(tree.getroot(), encoding="unicode"))
        private_write(self.path / "summary.html", render_html(data))


def render_html(data):
    def text(value):
        return escape(redact(value), quote=True)
    counts = data["counts"]
    cards = "".join("<div class=card><span>" + text(name.replace("_", " "))
                    + "</span><strong>" + str(value) + "</strong></div>"
                    for name, value in [("selected", len(data["cases"])), *counts.items(), ("deselected", data["deselected"])])
    rows = []
    for case in data["cases"]:
        outcome = case["outcome"]
        phases = []
        for phase in case["phases"]:
            logs = "\n\n".join(log["name"] + "\n" + log["text"] for log in phase["logs"])
            phases.append("<h4>" + text(phase["name"]) + " · " + text(phase["outcome"]) + "</h4><pre>" + text(phase["error"] + "\n" + logs) + "</pre>")
        meta = json.dumps(dict(component=case["component"], suite_level=case.get("level"),
                               test_type=case.get("kind"), identity=case["metadata"],
                               inputs_and_expectations=case["parameters"]), indent=2)
        opened = " open" if outcome in ("failed", "error") else ""
        component = text(case["component"])
        duration = format(case["duration"], ".3f")
        rows.append("<article class=case data-status=\"" + text(outcome) + "\" data-component=\"" + component + "\"><details" + opened + "><summary>"
                    + "<span class=badge>" + text(outcome.replace("_", " ")) + "</span><span class=test-name>" + text(case["nodeid"])
                    + "</span><span>" + component + " · " + text(case.get("level") or "—") + "</span><span>" + duration + " s"
                    + "</span></summary><div class=detail><h4>Case inputs and expected values</h4><pre>" + text(meta) + "</pre>"
                    + stage_table(case) + "<details class=phase-diagnostics><summary>Full phase diagnostics</summary>"
                    + "".join(phases) + "</details></div></details></article>")
    segments = "".join(f"<span class={name} style=flex-grow:{count}>" + text(name.replace("_", " ")) + f" {count}</span>" for name, count in counts.items() if count)
    executed = counts["passed"] + counts["failed"] + counts["error"]
    rate = format(100 * counts["passed"] / executed, ".1f") + "%" if executed else "—"
    errors = "\n\n".join([data["run_error"], *data["collection_errors"]]).strip()
    template = Path(__file__).with_name("report_template.html").read_text(encoding="utf-8")

    values = {
        "PRODUCER": text(data.get("producer", "Test controller")),
        "DISCLAIMER": text(data.get("disclaimer", "")),
        "TITLE": text(" + ".join(data["components"]).upper()), "STATUS": text(data["status"]),
        "RUN": text(data["run_id"]), "START": text(readable_timestamp(data["started"])), "END": text(readable_timestamp(data["finished"])),
        "CONSOLE": text(data.get("console", "full")),
        "DURATION": str(data["duration"]), "MODE": text(data["mode"]), "EXIT": str(data["exitcode"]),
        "CARDS": cards, "ROWS": "".join(rows) or "<p>No test cases were collected.</p>",
        "SEGMENTS": segments or "<span>No results</span>", "RATE": rate,
        "EXECUTED": str(executed), "SELECTION": text(json.dumps(data["selection"], indent=2)),
        "ERROR": "<section class=alert><h2>Run errors</h2><pre>" + text(errors) + "</pre></section>" if errors else "",
        "BACKENDS": text("\n".join(data["backend"])) or "No backend started (remote, preview, or initialization failure).",
    }
    return re.sub(r"@@([A-Z]+)@@", lambda match: values[match.group(1)], template)


_context = ContextVar("hal_execution_stages", default=None)

def begin(events):
    return _context.set((events, perf_counter()))

def end(token):
    _context.reset(token)

def emit(name, details="", outcome="info", duration=None):
    context = _context.get()
    if context is None:
        return
    events, started = context
    events.append(sanitized(dict(name=name, details=details, outcome=outcome,
        timestamp=utcnow(), elapsed=round(perf_counter() - started, 6), duration=duration)))

def assert_equal(actual, expected, name, message=None):
    """Report a real assertion, not transport completion."""
    passed = actual == expected
    emit("ASSERT", f"{name}: expected={expected!r} actual={actual!r}",
         "passed" if passed else "failed")
    assert passed, message or f"Expected {name}={expected!r}, got {actual!r}"


def detail(value):
    if isinstance(value, (dict, list)):
        value = json.dumps(sanitized(value), ensure_ascii=True)
    return redact(value)


def compact_case(case, index, total):
    title = detail(case["nodeid"])
    lines = [f"[{index}/{total}] {title}"]
    lines.append("  CONTEXT     " + " | ".join(str(case.get(k) or "unclassified")
                 for k in ("component", "level", "kind")))
    for event in case.get("stages", []):
        if event["name"] in ("RAFT", "TRANSPORT"):
            continue
        value = " ".join(detail(event["details"]).split())
        if len(value) > 360:
            value = value[:357] + "..."
        outcome = event["outcome"].upper()
        duration = event.get("duration")
        suffix = f" | {duration:.3f}s" if duration is not None else ""
        label = event["name"]
        when = event["timestamp"]
        lines.append(f"  {label:<11} {outcome} {value}{suffix} | {when}")
    return "\n".join(lines)


def stage_table(case):
    rows = []
    for event in case.get("stages", []):
        name = escape(detail(event["name"]))
        value = escape(detail(event["details"]))
        outcome = escape(detail(event["outcome"]))
        when = escape(detail(event["timestamp"]))
        duration = event.get("duration")
        timing = f"{duration:.3f} s" if duration is not None else "—"
        if event["name"] in ("COMMAND", "RESPONSE") or len(value) > 240:
            value = f"<details><summary>View {name.lower()}</summary><pre>{value}</pre></details>"
        rows.append(f"<tr data-outcome=\"{outcome}\"><th scope=row>{name}</th>"
                    f"<td>{value}</td><td>{outcome.upper()}</td>"
                    f"<td>{timing}</td><td>{when}</td></tr>")
    if not rows:
        return "<p class=muted>No execution stages recorded (collection only or not run).</p>"
    return ("<div class=stage-scroll><table class=stage-table><caption>Execution stages</caption>"
            "<thead><tr><th scope=col>Stage</th><th scope=col>Details</th>"
            "<th scope=col>Outcome</th><th scope=col>Duration</th><th scope=col>UTC timestamp</th>"
            "</tr></thead><tbody>" + "".join(rows) + "</tbody></table></div>")


class ReportPlugin:
    def __init__(self, directory, work_directory):
        self.directory = Path(directory)
        self.path = Path(work_directory) / "events.json"
        self.data = dict(cases=[], collected=0, deselected=0, collection_errors=[])
        self.cases = {}
        self.terminal = None

    def diagnostics(self, title, text):
        if os.environ.get(CONSOLE_ENV) != "status-only" or not text:
            return
        append_console(self.directory, "Saved diagnostics: " + title, text)

    def save(self):
        private_write(self.path, json.dumps(sanitized(self.data), indent=2, default=str))

    def pytest_collection_finish(self, session):
        self.terminal = session.config.pluginmanager.get_plugin("terminalreporter")
        self.data["collected"] = len(session.items)
        for item in session.items:
            case_marker = item.get_closest_marker("hal_case")
            params = getattr(getattr(item, "callspec", None), "params", {})
            case = dict(nodeid=item.nodeid, component=component_of(item),
                        level=level_of(item), kind="dynamic" if item.get_closest_marker("profile") else "static",
                        stages=[],
                        metadata=case_marker.kwargs if case_marker else {},
                        parameters=sanitized(params),
                        outcome="not_run", duration=0.0, phases=[], details="")
            self.cases[item.nodeid] = case
            self.data["cases"].append(case)
        self.save()

    @pytest.hookimpl(hookwrapper=True, tryfirst=True)
    def pytest_runtest_protocol(self, item, nextitem):
        case = self.cases.get(item.nodeid)
        if case is None:
            yield
            return
        token = begin(case["stages"])
        try:
            yield
        finally:
            completed = any(p["name"] == "teardown" for p in case["phases"])
            if not completed and case["outcome"] not in ("failed", "error"):
                case["outcome"] = "not_run"
                case["details"] += "Execution stopped before teardown completed\n"
            emit("RESULT", "Final testcase outcome after executed phases",
                 case["outcome"], case["duration"])
            end(token)
            self.save()
            index = self.data["cases"].index(case) + 1
            block = compact_case(case, index, self.data["collected"])
            if os.environ.get(CONSOLE_ENV) == "status-only":
                append_console(self.directory, "Execution stages", block)
            elif self.terminal:
                self.terminal.write_line("\n" + block)

    def pytest_deselected(self, items):
        self.data["deselected"] += len(items)
        self.save()

    def pytest_collectreport(self, report):
        if report.failed:
            self.data["collection_errors"].append(redact(report.longreprtext))
            self.diagnostics("collection " + report.nodeid, report.longreprtext)
            self.save()

    @pytest.hookimpl(hookwrapper=True)
    def pytest_runtest_makereport(self, item, call):
        outcome = yield
        report = outcome.get_result()
        if report.when != "teardown":
            return
        marker = item.get_closest_marker("hal_case")
        case = self.cases.get(item.nodeid, {})
        report.user_properties.append(("component", str(case.get("component", "other"))))
        report.user_properties.append(("suite_level", str(case.get("level") or "unclassified")))
        if marker:
            report.user_properties.extend((key, str(value)) for key, value in sanitized(marker.kwargs).items())
        params = getattr(getattr(item, "callspec", None), "params", {})
        report.user_properties.extend(("parameter." + key, str(value)) for key, value in sanitized(params).items())

    def pytest_runtest_logreport(self, report):
        case = self.cases.get(report.nodeid)
        if case is None:
            return
        phase = dict(name=report.when, outcome=report.outcome, duration=report.duration,
                     logs=[dict(name=name, text=redact(text)) for name, text in report.sections],
                     error=redact(report.longreprtext) if report.longrepr else "")
        logs = [name + "\n" + text for name, text in report.sections
                if name.endswith(" " + report.when)]
        diagnostic = "\n\n".join([phase["error"], *logs]).strip()
        self.diagnostics(report.nodeid + " [" + report.when + "]", diagnostic)
        emit(report.when.upper(), phase["error"] if report.failed or report.skipped else "",
             "error" if report.failed and report.when != "call" else report.outcome, report.duration)
        case["phases"].append(phase)
        case["duration"] += report.duration
        if report.failed:
            case["outcome"] = "failed" if report.when == "call" else "error"
            case["details"] += phase["error"] + "\n"
        elif report.skipped and case["outcome"] not in ("failed", "error"):
            case["outcome"] = "skipped"
            case["details"] = phase["error"]
        elif report.when == "call" and case["outcome"] == "not_run":
            case["outcome"] = "passed"
        self.save()


def pytest_configure(config):
    directory = os.environ.get(RUN_ENV)
    if directory:
        work_directory = os.environ.get(WORK_ENV)
        if not work_directory:
            raise pytest.UsageError("Missing runner reporting workspace")
        if os.environ.get(CONSOLE_ENV) == "status-only" and not config.option.collectonly:
            config.option.verbose = max(1, config.option.verbose)
        config.pluginmanager.register(ReportPlugin(directory, work_directory), "hal-run-events")

