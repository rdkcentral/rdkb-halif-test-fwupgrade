"""VL backend support, testcase helpers, and isolated RAFT execution."""
from contextlib import contextmanager
import json
import logging
import os
from pathlib import Path
import re
import shlex
import socket
import struct
import time
import subprocess
import sys
import tempfile
from time import perf_counter
from .configuration import get_settings
from .reporting import RUN_ENV, assert_equal, emit, private_write, redact

logger = logging.getLogger(__name__)


# VL wire protocol (mirrors cli/vl_generic.h).
# Header: three network-order uint32 words (magic, type, size).
FRAME_HEADER_FORMAT = "!III"
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER_FORMAT)
VL_MSG_ACK = 2
VL_MSG_REQUEST = 3
VL_MSG_RESPONSE = 4
VL_REQ_HAL = 1
VL_MAX_PAYLOAD = 8192
VL_STATUS_OK = 0
# Request payload: type byte + command text + NUL; response: status byte + text + NUL.
LIST_APIS_PAYLOAD = bytes([VL_REQ_HAL]) + b"--list-apis\0"
MIN_RESPONSE_SIZE = 2
PROBE_TIMEOUT_SECONDS = 0.5

# RAFT bridge execution limits.
MIN_COMMAND_TIMEOUT_SECONDS = 1
MAX_COMMAND_TIMEOUT_SECONDS = 300
BRIDGE_STARTUP_GRACE_SECONDS = 90
MAX_EXIT_CODE = 255
PRIVATE_FILE_UMASK = 0o077
PRIVATE_DIR_MODE = 0o700


# VL command construction and backend readiness

def responsive(component):
    # Probe protocol identity, not merely an accepting socket.
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as conn:
            deadline = time.monotonic() + PROBE_TIMEOUT_SECONDS
            conn.settimeout(PROBE_TIMEOUT_SECONDS)
            conn.connect(component.socket_path)
            payload = LIST_APIS_PAYLOAD
            def exact(size):
                result = b""
                while len(result) < size:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise OSError("Probe deadline expired")
                    conn.settimeout(remaining)
                    chunk = conn.recv(size - len(result))
                    if not chunk:
                        raise OSError("Incomplete probe")
                    result += chunk
                return result
            magic = component.options["magic"]
            conn.sendall(struct.pack(FRAME_HEADER_FORMAT, magic, VL_MSG_REQUEST, len(payload)))
            if struct.unpack(FRAME_HEADER_FORMAT, exact(FRAME_HEADER_SIZE)) != (magic, VL_MSG_ACK, 0):
                return False
            conn.sendall(payload)
            reply_magic, kind, size = struct.unpack(FRAME_HEADER_FORMAT, exact(FRAME_HEADER_SIZE))
            if (reply_magic != magic or kind != VL_MSG_RESPONSE
                    or not MIN_RESPONSE_SIZE <= size <= VL_MAX_PAYLOAD):
                return False
            conn.sendall(struct.pack(FRAME_HEADER_FORMAT, magic, VL_MSG_ACK, 0))
            data = exact(size)
            return data[0] == VL_STATUS_OK and data[-1] == 0 and b"status: 0\n" in data and b"api: " in data
    except (OSError, ValueError):
        return False


def startup(component):
    return [str(component.server), f"--socket={component.socket_path}"]

def build_command(component, api, parameters_json=None, *, output_pointer=None, parameters=None):
    """Build one shared-CLI request with explicit component routing settings."""
    if parameters is not None:
        if not isinstance(parameters, dict):
            raise ValueError("parameters must be a field-name mapping")
        if parameters and parameters_json is not None:
            raise ValueError("Cannot mix JSON and named CLI parameters")
        for name, value in parameters.items():
            if (not isinstance(name, str)
                    or re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_-]*", name) is None
                    or name in {"json", "socket", "magic", "component", "api-option", "api", "help", "output-pointer", "list-apis"}
                    or name.endswith("-api")
                    or "--" + name == component.options["selector"]):
                raise ValueError("Invalid or reserved CLI field name")
            if (not isinstance(value, (str, int, float)) or isinstance(value, bool)
                    or chr(0) in str(value)):
                raise ValueError("CLI field values must be strings or numbers without NUL")
    settings = component
    args = [
        settings.cli,
        f"--component={settings.name}",
        f"--socket={settings.socket_path}",
        "--api=" + api,
    ]
    if parameters_json is not None:
        args.append(f"--json={parameters_json}")
    for name, value in (parameters or {}).items():
        args.append(f"--{name}={value}")
    if output_pointer is not None:
        args.append(f"--output-pointer={output_pointer}")
    # Quote each argv item for the target POSIX shell, preserving JSON quotes
    # and keeping spaces/metacharacters inside the original argument.
    return shlex.join(args)


def preflight(component):
    if os.path.lexists(component.socket_path):
        raise RuntimeError(f"{component.name} socket exists but is not responsive; inspect manually")


# Testcase execution and state restoration

# Used by: L1, L2, and future levels.
def command(component, api, parameters_json=None, *, output_pointer=None, parameters=None):
    """Delegate request encoding to the configured component adapter."""
    settings = get_settings().components[component]
    return settings.command_builder(settings, api, parameters_json,
        output_pointer=output_pointer, parameters=parameters)

# Used by: L1, L2, and future levels.
def run(component, raft_env, api, parameters_json=None, *, output_pointer=None, parameters=None):
    """Execute the constructed CLI command using the configured RAFT transport.

    Return {"output": text, "returncode": int}; nonzero CLI exits remain data
    so negative tests can inspect them. Transport/capture failures raise.
    This does not start the DUT backend; provision it or use the app runner.
    """
    payload = parameters_json
    if parameters_json is not None:
        try:
            payload = json.loads(parameters_json)
        except (ValueError, TypeError):
            pass  # Malformed JSON may be an intentional test input.
    inputs = {"api": api}
    if parameters_json is not None:
        inputs["payload"] = payload
    if parameters is not None and parameters != {}:
        inputs["parameters"] = parameters
    if output_pointer is not None:
        inputs["output_pointer"] = output_pointer
    emit("INPUT", inputs)
    try:
        cmd = command(component, api, parameters_json, output_pointer=output_pointer,
                      parameters=parameters)
    except Exception as exc:
        emit("COMMAND", str(exc), "error")
        raise
    emit("COMMAND", cmd)
    logger.info("CLI Command: %s", cmd)
    started = perf_counter()
    try:
        result = raft_env.run_command_result(cmd)
    except Exception as exc:
        emit("RAFT", str(exc), "error", perf_counter() - started)
        raise
    code = result["returncode"]
    emit("RAFT", f"capture=complete | cli_exit={code} (observed)",
         "complete", perf_counter() - started)
    emit("RESPONSE", result["output"])
    logger.info("CLI Response:\n%s", result["output"])
    return result

# Used by: L1, L2, and future levels.
def field(response, name):
    """Read exactly one integer field from CLI text, e.g. status: 0."""
    values = re.findall(rf"(?m)^{re.escape(name)}:[ \t]*(-?\d+)[ \t]*\r?$", response)
    if len(values) != 1:
        emit("PARSE", f"Expected exactly one integer {name} field", "error")
    assert len(values) == 1, f"Expected exactly one integer {name} field"
    return int(values[0])


# Used by: tests that must snapshot/restore text state (URL, filename, ...).
def text_field(response, name):
    """Read exactly one single-line text field from CLI text, e.g. url: https://host/x."""
    values = re.findall(rf"(?m)^{re.escape(name)}:[ \t]*(.*?)[ \t]*\r?$", response)
    if len(values) != 1:
        emit("PARSE", f"Expected exactly one text {name} field", "error")
    assert len(values) == 1, f"Expected exactly one text {name} field"
    return values[0]


@contextmanager
def restore_fields(component, raft_env, *, get_api, set_api, fields,
                   restore_as_json=False, text_fields=None):
    """Save GET fields and restore them after the testcase block.

    fields maps SET parameter names to integer GET response field names.
    text_fields (optional) does the same for text-valued fields; pass
    fields={} when only text fields are restored. Snapshot and cleanup
    require API status zero; test-result assertions stay in the caller. No
    CLI-exit or value-readback checks are performed. If both the test and
    cleanup fail, exception chaining retains both tracebacks.
    """
    text_fields = text_fields or {}
    mappings = (fields, text_fields)
    if (any(not isinstance(mapping, dict) for mapping in mappings)
            or not (fields or text_fields)
            or set(fields) & set(text_fields)
            or any(not isinstance(key, str) or not key
                   or not isinstance(value, str) or not value
                   for mapping in mappings for key, value in mapping.items())):
        raise ValueError("fields must map SET parameter names to GET field names")
    snapshot = run(component, raft_env, get_api)
    if field(snapshot["output"], "status") != 0:
        raise RuntimeError(f"Snapshot failed: {get_api}")
    original = {
        parameter: field(snapshot["output"], response_field)
        for parameter, response_field in fields.items()
    }
    original.update({
        parameter: text_field(snapshot["output"], response_field)
        for parameter, response_field in text_fields.items()
    })
    test_error = None
    try:
        yield
    except Exception as error:
        test_error = error
        raise
    finally:
        try:
            if restore_as_json:
                restored = run(
                    component, raft_env, set_api,
                    parameters_json=json.dumps(original, separators=(",", ":")),
                )
            else:
                restored = run(component, raft_env, set_api, parameters=original)
            if field(restored["output"], "status") != 0:
                raise RuntimeError(f"Restoration failed: {set_api}")
        except Exception as cleanup_error:
            if test_error is not None:
                raise cleanup_error from test_error
            raise


@contextmanager
def _raft_logs(settings, component):
    """Keep private, redacted bridge/native logs outside console capture."""
    root = settings.root / "logs"
    run = os.environ.get(RUN_ENV)
    parent = Path(run) if run else root
    if not parent.resolve().is_relative_to(root.resolve()):
        raise RuntimeError("RAFT logs must remain under the controller logs folder")
    parent = parent / "raft"
    parent.mkdir(parents=True, exist_ok=True, mode=PRIVATE_DIR_MODE)
    directory = Path(tempfile.mkdtemp(prefix=component + "-", dir=parent))
    try:
        yield directory
    finally:
        # Native RAFT handlers can include console/session details too.
        for path in directory.rglob("*"):
            if path.is_file() and not path.is_symlink():
                private_write(path, redact(path.read_text(encoding="utf-8", errors="replace")))


# RAFT environment (the bridge itself remains in raft_bridge.py)

class RaftEnvironment:
    """Execute each command in a fresh RAFT console session on the DUT.

    The fixture passes the rack config selected by RAFT_CONFIG. With an SSH
    console in that config, raft_bridge.py sends the command over SSH; this
    class itself only starts the local RAFT bridge subprocess.
    """

    def __init__(self, config, component):
        if component not in get_settings().components:
            raise ValueError("Unknown component")
        self.component = component
        self.settings = get_settings()
        self.config = Path(config).expanduser().resolve()
        if not self.config.is_file():
            raise FileNotFoundError(f"RAFT config not found: {self.config}")

    # PUBLIC_INTERFACE
    def run_command_result(self, command, timeout=30):
        """Return output and remote returncode after verified command completion.

        command is the shell-safe string built by execution.command(), including
        any adapter-encoded payload. Pass it through unchanged: do not parse,
        reserialize, or manually escape the request JSON at the transport layer.

        A nonzero remote exit code is data, not a bridge failure. Transport,
        timeout, incomplete capture and malformed-result errors always raise.
        """
        if not isinstance(command, str) or not command.strip() or chr(0) in command:
            raise ValueError("command must be nonempty and contain no NUL")
        if (type(timeout) is not int
                or not MIN_COMMAND_TIMEOUT_SECONDS <= timeout <= MAX_COMMAND_TIMEOUT_SECONDS):
            raise ValueError(
                f"timeout must be an integer between {MIN_COMMAND_TIMEOUT_SECONDS} "
                f"and {MAX_COMMAND_TIMEOUT_SECONDS}")
        def diagnostics(stdout="", stderr=""):
            def text(value):
                return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else (value or "")
            captured = text(stdout) + "\n" + text(stderr)
            if captured.strip():
                identity = os.environ.get("PYTEST_CURRENT_TEST", "capability discovery")
                private_write(log_dir / "bridge.log", redact(
                    f"--- RAFT diagnostics: {self.component} | {identity} ---\n" + captured))
        with tempfile.TemporaryDirectory(prefix="raft-command-") as directory, \
                _raft_logs(self.settings, self.component) as log_dir:
            # Controller-local bridge results, NOT the API request payload.
            # Inline request JSON stays inside RAFT_BRIDGE_COMMAND.
            path = Path(directory) / "result.json"
            # Transfer the command through the environment, not a local shell.
            env = os.environ.copy()
            env.update(RAFT_BRIDGE_RUNTIME=str(self.settings.raft_root),
                       RAFT_BRIDGE_COMMAND=command, RAFT_BRIDGE_TIMEOUT=str(timeout),
                       RAFT_BRIDGE_RESULT=str(path),
                       RAFT_BRIDGE_LOG_DIR=str(log_dir))
            try:
                result = subprocess.run(
                    [sys.executable, str(Path(__file__).with_name("raft_bridge.py")),
                     "--config", str(self.config)], cwd=self.settings.root, env=env,
                    capture_output=True, text=True,
                    timeout=timeout + BRIDGE_STARTUP_GRACE_SECONDS, umask=PRIVATE_FILE_UMASK)
            except subprocess.TimeoutExpired as exc:
                diagnostics(exc.stdout, exc.stderr)
                raise RuntimeError("RAFT bridge timeout") from None
            except OSError:
                diagnostics(stderr="Could not start RAFT bridge")
                raise RuntimeError("RAFT bridge transport/process failure") from None
            diagnostics(result.stdout, result.stderr)
            if not path.is_file():
                raise RuntimeError("RAFT bridge incomplete capture: missing result")
            try:
                payload = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, UnicodeError, ValueError):
                raise RuntimeError("RAFT bridge malformed result") from None
            if not isinstance(payload, dict):
                raise RuntimeError("RAFT bridge malformed result")
            transport = payload.get("transport", {})
            if isinstance(transport, dict):
                observed = {key: transport[key] for key in ("shell_ready", "command_sent",
                            "capture_completed", "session_close_completed")
                            if type(transport.get(key)) is bool}
                if observed:
                    emit("TRANSPORT", observed, "complete" if all(observed.values()) else "error")
            if result.returncode != 0 or payload.get("passed") is not True:
                # Never expose command output, rack details or credentials here.
                error = payload.get("error")
                if (not isinstance(error, str)
                        or error not in {"transport", "timeout", "incomplete_capture",
                                         "malformed_capture", "raft_failure"}):
                    error = "transport"
                raise RuntimeError(f"RAFT bridge failure: {error}")
            code = payload.get("returncode")
            if (payload.get("completed") is not True or type(code) is not int
                    or not 0 <= code <= MAX_EXIT_CODE
                    or not isinstance(payload.get("output"), str)):
                raise RuntimeError("RAFT bridge malformed or incomplete result")
            return {"output": payload["output"], "returncode": code}

    # PUBLIC_INTERFACE
    def run_command(self, command, timeout=30):
        """Return output only on remote exit zero, preserving the original API."""
        result = self.run_command_result(command, timeout)
        if result["returncode"] != 0:
            raise RuntimeError(
                f"RAFT remote command failed with exit code {result['returncode']}"
            )
        return result["output"]

