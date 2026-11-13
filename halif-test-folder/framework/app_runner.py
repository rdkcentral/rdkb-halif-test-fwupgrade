"""Explicit same-host lifecycle; remote mode never starts local servers."""
import argparse
import importlib
from contextlib import ExitStack
import os
from pathlib import Path
import subprocess
import sys
import time
from .reporting import (
    CONSOLE_ENV, RUN_ENV, WORK_ENV, XML_FILENAME, RunReport, append_console,
    readable_timestamp, redact,
)
from .core import SELECTION_OPTIONS, add_selection_options, resolve_selection
from .configuration import configure, get_settings

def stop_process(process, timeout=15):
    """Reap a child process, escalating only when graceful shutdown times out."""
    forced = False
    if process.poll() is None:
        try:
            process.terminate()
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()
        forced = True
    return forced


class Backend:
    def __init__(self, component, report=None):
        self.component, self.process, self.log = component, None, None
        self.report = report
        self.log_path = None

    def start(self):
        c = self.component
        if c.readiness(c):
            print(f"Reusing externally owned {c.name} backend (service logs not collected)", flush=True)
            if self.report:
                self.report.data["backend"].append(f"{c.name}: reused external backend; service logs unavailable")
            return self
        c.preflight(c)
        if not c.server.is_file() or not os.access(c.server, os.X_OK):
            raise RuntimeError(f"Build backend first: {c.server}")
        log_path = self.report.work_path / "backend" / f"{c.name}.log" if self.report else c.log
        self.log_path = log_path
        log_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.log = os.fdopen(os.open(log_path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600), "a")
        if self.report:
            self.report.data["backend"].append(f"{c.name}: runner-owned; diagnostics saved in console.log")
        print(f"Starting {c.name} backend", flush=True)
        self.process = subprocess.Popen(c.startup(c),
                                        stdout=self.log, stderr=subprocess.STDOUT, umask=0o077)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(f"{c.name} startup failed; inspect console.log")
            if c.readiness(c):
                return self
            time.sleep(0.1)
        raise RuntimeError(f"{c.name} readiness timeout")

    def close(self):
        try:
            if self.process is not None:
                if self.process.poll() is None:
                    print(f"Stopping owned {self.component.name} backend", flush=True)
                if stop_process(self.process):
                    print("Forced shutdown; inspect backend resources manually", file=sys.stderr)
        finally:
            try:
                if self.log is not None:
                    self.log.close()
                if (self.report is not None and self.log_path is not None
                        and self.log_path.is_file()):
                    append_console(
                        self.report.path,
                        f"Backend diagnostics: {self.component.name}",
                        self.log_path.read_text(encoding="utf-8", errors="replace"),
                    )
            finally:
                self.process, self.log, self.log_path = None, None, None

def execute(components, pytest_args, *, mode, report=None):
    # Inline JSON changes only CLI arguments, not backend lifecycle:
    # same-host manages local backends and requires RAFT to target this host;
    # remote assumes the target backend is provisioned and already running.
    # Direct python -m pytest bypasses this function and starts no backend.
    with ExitStack() as cleanup:
        if mode == "same-host" and not any(arg in ("--collect-only", "--co") for arg in pytest_args):
            for component in components:
                backend = Backend(component, report)
                cleanup.callback(backend.close)  # Register before startup can fail.
                backend.start()
        env = os.environ.copy()
        env["CI"] = "1"
        if report:
            env[RUN_ENV] = str(report.path)
            env[WORK_ENV] = str(report.work_path)
            env[CONSOLE_ENV] = report.data.get("console", "full")
        return stream_pytest(pytest_args, env)


def stream_pytest(pytest_args, env):
    process = subprocess.Popen([sys.executable, "-m", "pytest", *pytest_args], cwd=get_settings().root,
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, encoding="utf-8", errors="replace", bufsize=1,
                               umask=0o077)
    try:
        for line in process.stdout:
            print(line, end="", flush=True)
        return process.wait()
    finally:
        stop_process(process)
        process.stdout.close()


def main(argv=None, *, settings=None):
    if settings is None:
        bootstrap = argparse.ArgumentParser(description="Supply a project settings module, or use its runner entry point", allow_abbrev=False, add_help=False)
        bootstrap.add_argument("--settings", default="framework.configuration", help="Module exposing create_settings()")
        supplied, argv = bootstrap.parse_known_args(argv)
        settings = importlib.import_module(supplied.settings).create_settings()
    configure(settings)
    ROOT, COMPONENTS = settings.root, settings.components
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    add_selection_options(parser.add_argument)
    parser.add_argument("--mode", choices=["same-host", "remote"], required=True,
                        help="same-host: manage local backends; RAFT must execute on this host. "
                             "remote: use already provisioned backends on the RAFT target")
    parser.add_argument("--console", choices=["full", "status-only"], default="full",
                        help="Terminal output: compact execution stages (full, default) or testcase statuses only; detailed report diagnostics are always saved")
    parser.add_argument("--config", default=os.environ.get("RAFT_CONFIG"),
                        help="Approved rack YAML; overrides RAFT_CONFIG")
    args, pytest_args = parser.parse_known_args(argv)
    if pytest_args[:1] == ["--"]:
        pytest_args = pytest_args[1:]
    if any(arg.split("=", 1)[0] in (*SELECTION_OPTIONS, "--mode", "--console", "--config") for arg in pytest_args):
        parser.error("Select --component and other runner filters on the runner, not in forwarded pytest arguments")
    try:
        selection = resolve_selection(vars(args))
    except ValueError as error:
        parser.error(str(error))
    report = RunReport(ROOT / "logs", selection.components, args.mode,
                       dict(levels=selection.levels, api=selection.api,
                            category=selection.case_type, case_id=selection.case_id),
                       collection=any(arg in ("--collect-only", "--co") for arg in pytest_args))
    report.data["console"] = args.console
    code, error = 1, None
    with report.workspace, report.console() as logfile:
        stdout, stderr = report.capture(logfile)
        with stdout, stderr:
            print(f"RUN START {readable_timestamp(report.started)} | {report.path.name}", flush=True)
            print(f"Reports: {report.path}", flush=True)
            try:
                config = args.config
                if not report.data["collection"] and (not config or not Path(config).expanduser().is_file()):
                    raise RuntimeError("Supply RAFT_CONFIG pointing to an existing approved rack configuration")
                if config:
                    os.environ["RAFT_CONFIG"] = str(Path(config).expanduser().resolve())
                targets = " ".join(str(path.relative_to(ROOT)) for path in selection.test_paths(ROOT))
                forwarded = ["-o", f"testpaths={targets}", *selection.pytest_args(), *pytest_args,
                             "-p", "framework.reporting", "--color=no",
                             "--junitxml=" + str(report.path / XML_FILENAME),
                             "-o", "junit_family=xunit1", "-o", "junit_logging=all",
                             "-o", "junit_log_passing_tests=true", "-o", "log_level=INFO",
                             "-o", "log_cli_level=INFO"]
                forwarded += ["--capture=fd", "-o", "log_cli=false"]
                if args.console == "status-only":
                    forwarded += ["--tb=no", "--show-capture=no", "-rN"]
                code = execute([COMPONENTS[n] for n in selection.components], forwarded,
                               mode=args.mode, report=report)
            except KeyboardInterrupt:
                code, error = 130, "Run interrupted by user"
            except Exception as exc:
                code, error = 1, redact(str(exc))
                print(f"Runner failed: {error}", file=sys.stderr)
            finally:
                try:
                    report.finish(code, error)
                except Exception as exc:
                    print("Could not finalize reports: " + redact(str(exc)), file=sys.stderr)
                    if code == 0:
                        code = 1
                status = report.data.get("status", "REPORT ERROR")
                print(f"RUN END | {status} | exit {code}")
                for name in ("console.log", "summary.html", XML_FILENAME):
                    print(f"  {report.path / name}")
    return code

if __name__ == "__main__":
    sys.exit(main())

