# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
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
from .reporting import (RUN_ENV, assert_equal, emit, mask_fields, private_write, redact,
                        register_secret)

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
# vl_cli --timeout is set this much below the RAFT budget so the CLI reports its own
# timeout before the bridge gives up on the whole command.
CLI_TIMEOUT_MARGIN_SECONDS = 5
# Reboot-aware calls (see run_reboot): settle after the call, then poll the backend.
REBOOT_RECOVERY_DEFAULTS = {"settle_seconds": 5, "timeout_seconds": 300,
                            "poll_interval_seconds": 5}
BOOT_ID_COMMAND = "cat /proc/sys/kernel/random/boot_id"
BOOT_ID_PATTERN = re.compile(r"[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}")
BACKEND_PROBE_TIMEOUT_SECONDS = 15
# A RAFT session that fails before the command is sent (SSH shell not ready) is retried:
# nothing ran on the DUT, so repeating it is safe. Total attempts = 1 + retries.
BRIDGE_PRESEND_RETRIES = 2
BRIDGE_PRESEND_RETRY_DELAY_SECONDS = 2
# Profile "factory_reset_recovery": reprovisioning hooks for destructive resets.
REPROVISION_DEFAULTS = {"host_command": "", "dut_command": "",
                        "command_timeout_seconds": 60, "backend_grace_seconds": 30}
# Set when a destructive call left the DUT unrecovered; later tests are skipped.
_UNRECOVERED = []
MAX_EXIT_CODE = 255
PRIVATE_FILE_UMASK = 0o077
PRIVATE_DIR_MODE = 0o700


class BridgeNotSentError(RuntimeError):
    """RAFT bridge failure before the command reached the DUT (safe to retry)."""


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

def _check_timeout(timeout_seconds):
    if (type(timeout_seconds) is not int
            or not MIN_COMMAND_TIMEOUT_SECONDS <= timeout_seconds <= MAX_COMMAND_TIMEOUT_SECONDS):
        raise ValueError(
            f"timeout_seconds must be an integer between {MIN_COMMAND_TIMEOUT_SECONDS} "
            f"and {MAX_COMMAND_TIMEOUT_SECONDS}")


def build_command(component, api, parameters_json=None, *, output_pointer=None, parameters=None,
                  timeout_seconds=None):
    """Build one shared-CLI request with explicit component routing settings.

    timeout_seconds, when given, becomes the CLI's own --timeout (transport deadline).
    """
    if timeout_seconds is not None:
        _check_timeout(timeout_seconds)
    if parameters is not None:
        if not isinstance(parameters, dict):
            raise ValueError("parameters must be a field-name mapping")
        if parameters and parameters_json is not None:
            raise ValueError("Cannot mix JSON and named CLI parameters")
        for name, value in parameters.items():
            if (not isinstance(name, str)
                    or re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_-]*", name) is None
                    or name in {"json", "socket", "magic", "component", "api-option", "api", "help", "output-pointer", "list-apis", "timeout"}
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
    if timeout_seconds is not None:
        args.append(f"--timeout={timeout_seconds}")
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
def command(component, api, parameters_json=None, *, output_pointer=None, parameters=None,
            timeout_seconds=None):
    """Delegate request encoding to the configured component adapter."""
    settings = get_settings().components[component]
    extra = {} if timeout_seconds is None else {"timeout_seconds": timeout_seconds}
    return settings.command_builder(settings, api, parameters_json,
        output_pointer=output_pointer, parameters=parameters, **extra)

def sensitive_fields(component, api):
    """Response fields of api that carry secrets, from the component's options.

    A component declares them as options["sensitive_output"] = {api: ["field", ...]} (or
    {api: "*"} for every field except status). Returns None for ordinary APIs.
    """
    declared = get_settings().components[component].options.get("sensitive_output") or {}
    fields = declared.get(api)
    return fields if fields else None


# Used by: L1, L2, and future levels.
def run(component, raft_env, api, parameters_json=None, *, output_pointer=None, parameters=None,
        timeout_seconds=None):
    """Execute the constructed CLI command using the configured RAFT transport.

    timeout_seconds is the whole-operation budget (1..300) for long firmware calls such as
    download or install. It is the RAFT command timeout, and the CLI receives
    --timeout=<budget - CLI_TIMEOUT_MARGIN_SECONDS> so it keeps waiting for the HAL result.
    Omit it for ordinary calls (CLI default 10 s, RAFT default 30 s).

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
    secret_fields = sensitive_fields(component, api)
    if secret_fields and isinstance(payload, dict):
        # Request values for sensitive fields must not reach logs or reports either.
        for name, value in payload.items():
            if secret_fields == "*" or name in secret_fields:
                register_secret(value)
    inputs = {"api": api}
    cli_timeout = None
    if timeout_seconds is not None:
        _check_timeout(timeout_seconds)
        cli_timeout = max(1, timeout_seconds - CLI_TIMEOUT_MARGIN_SECONDS)
        inputs["timeout_seconds"] = timeout_seconds
    if parameters_json is not None:
        inputs["payload"] = payload
    if parameters is not None and parameters != {}:
        inputs["parameters"] = parameters
    if output_pointer is not None:
        inputs["output_pointer"] = output_pointer
    emit("INPUT", inputs)
    try:
        cmd = command(component, api, parameters_json, output_pointer=output_pointer,
                      parameters=parameters, timeout_seconds=cli_timeout)
    except Exception as exc:
        emit("COMMAND", str(exc), "error")
        raise
    emit("COMMAND", cmd)
    logger.info("CLI Command: %s", cmd)
    started = perf_counter()
    try:
        result = (raft_env.run_command_result(cmd) if timeout_seconds is None
                  else raft_env.run_command_result(cmd, timeout=timeout_seconds))
    except Exception as exc:
        emit("RAFT", str(exc), "error", perf_counter() - started)
        raise
    code = result["returncode"]
    emit("RAFT", f"capture=complete | cli_exit={code} (observed)",
         "complete", perf_counter() - started)
    shown = result["output"]
    if secret_fields:
        # API-aware masking: the caller still gets the real output, but only the masked
        # text is emitted/logged, and the removed values are masked on every other path
        # (assertion messages, tracebacks, XML/HTML reports) by redact().
        shown, secrets = mask_fields(shown, secret_fields)
        for secret in secrets:
            register_secret(secret)
    emit("RESPONSE", shown)
    logger.info("CLI Response:\n%s", shown)
    return result

# Reboot-aware execution

def _recovery_settings(recovery):
    unknown = set(recovery or {}) - set(REBOOT_RECOVERY_DEFAULTS)
    if unknown:
        raise ValueError(f"Unknown reboot_recovery keys: {sorted(unknown)}")
    merged = {**REBOOT_RECOVERY_DEFAULTS, **(recovery or {})}
    for key, value in merged.items():
        minimum_ok = value >= 0 if key == "settle_seconds" else value > 0
        if not isinstance(value, (int, float)) or isinstance(value, bool) or not minimum_ok:
            kind = "non-negative" if key == "settle_seconds" else "positive"
            raise ValueError(f"reboot_recovery.{key} must be a {kind} number")
    return merged


def _reprovision_settings(reprovision):
    unknown = set(reprovision or {}) - set(REPROVISION_DEFAULTS)
    if unknown:
        raise ValueError(f"Unknown factory_reset_recovery keys: {sorted(unknown)}")
    merged = {**REPROVISION_DEFAULTS, **(reprovision or {})}
    for key in ("host_command", "dut_command"):
        if not isinstance(merged[key], str):
            raise ValueError(f"factory_reset_recovery.{key} must be a string")
    for key in ("command_timeout_seconds", "backend_grace_seconds"):
        value = merged[key]
        if not isinstance(value, (int, float)) or isinstance(value, bool) or value <= 0:
            raise ValueError(f"factory_reset_recovery.{key} must be a positive number")
    return merged


def unrecovered_reason():
    """Why the DUT is unusable after a failed recovery, or None while it is healthy."""
    return _UNRECOVERED[0] if _UNRECOVERED else None


def _mark_unrecovered(reason):
    if not _UNRECOVERED:
        _UNRECOVERED.append(reason)


def boot_id(raft_env):
    """DUT boot identifier (changes on every reboot), or None when it cannot be read."""
    for _ in range(2):
        try:
            result = raft_env.run_command_result(BOOT_ID_COMMAND,
                                                 timeout=BACKEND_PROBE_TIMEOUT_SECONDS)
        except RuntimeError:
            continue
        match = BOOT_ID_PATTERN.search(result["output"]) if result["returncode"] == 0 else None
        if match:
            return match.group(0).lower()
    return None


def wait_for_backend(component, raft_env, *, timeout_seconds, poll_interval_seconds):
    """Poll the DUT backend through fresh RAFT sessions until it answers --list-apis.

    Every RAFT command opens a new session, so this is also the reconnect wait after a
    reboot. Returns the number of probes; raises RuntimeError if the backend does not
    come back within timeout_seconds. Provisioning must restart the backend on boot.
    """
    settings = get_settings().components[component]
    probe = shlex.join([settings.cli, f"--component={settings.name}",
                        f"--socket={settings.socket_path}", "--list-apis"])
    deadline = time.monotonic() + timeout_seconds
    attempts = 0
    while True:
        attempts += 1
        try:
            result = raft_env.run_command_result(probe, timeout=BACKEND_PROBE_TIMEOUT_SECONDS)
            if result["returncode"] == 0 and "status: 0" in result["output"]:
                return attempts
        except RuntimeError:
            pass  # DUT still down or SSH not up yet.
        if time.monotonic() + poll_interval_seconds > deadline:
            raise RuntimeError(f"{component} backend did not recover within "
                               f"{timeout_seconds}s after the reboot call")
        time.sleep(poll_interval_seconds)


def wait_for_management(raft_env, *, timeout_seconds, poll_interval_seconds):
    """Wait until the DUT answers over RAFT/SSH again (boot id readable)."""
    deadline = time.monotonic() + timeout_seconds
    while boot_id(raft_env) is None:
        if time.monotonic() + poll_interval_seconds > deadline:
            raise RuntimeError(f"DUT management access did not return within "
                               f"{timeout_seconds}s; the rack configuration may no longer "
                               "match the DUT after the reset")
        time.sleep(poll_interval_seconds)


def _run_host_hook(command, timeout):
    """Run a reprovisioning command on the test host (no shell; output is redacted)."""
    try:
        done = subprocess.run(shlex.split(command), capture_output=True, text=True,
                              timeout=timeout, check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise RuntimeError(f"host reprovision command failed: {redact(str(exc))}") from exc
    shown = redact((done.stdout + done.stderr).strip())
    emit("REPROVISION", f"host command exit {done.returncode}: {shown}")
    if done.returncode != 0:
        raise RuntimeError(f"host reprovision command exited {done.returncode}: {shown}")


def _run_dut_hook(raft_env, command, timeout):
    """Run a reprovisioning command on the DUT (for example start the dispatcher)."""
    result = raft_env.run_command_result(command, timeout=timeout)
    shown = redact(result["output"].strip())
    emit("REPROVISION", f"DUT command exit {result['returncode']}: {shown}")
    if result["returncode"] != 0:
        raise RuntimeError(f"DUT reprovision command exited {result['returncode']}: {shown}")


def _recover_backend(component, raft_env, cfg, reprovision):
    """Wait for the backend; if it stays down, reprovision the DUT, then wait again."""
    wait = {"timeout_seconds": cfg["timeout_seconds"],
            "poll_interval_seconds": cfg["poll_interval_seconds"]}
    hooks = reprovision and (reprovision["host_command"] or reprovision["dut_command"])
    if not hooks:
        try:
            return wait_for_backend(component, raft_env, **wait)
        except RuntimeError as exc:
            if reprovision is None:
                raise
            raise RuntimeError(f"{exc}; set factory_reset_recovery.host_command or "
                               "dut_command to reprovision the DUT") from exc
    try:
        grace = {**wait, "timeout_seconds": reprovision["backend_grace_seconds"]}
        return wait_for_backend(component, raft_env, **grace)
    except RuntimeError:
        emit("REPROVISION", "backend still down after the reset; reprovisioning the DUT")
    limit = reprovision["command_timeout_seconds"]
    if reprovision["host_command"]:
        _run_host_hook(reprovision["host_command"], limit)
    wait_for_management(raft_env, timeout_seconds=cfg["timeout_seconds"],
                        poll_interval_seconds=cfg["poll_interval_seconds"])
    if reprovision["dut_command"]:
        _run_dut_hook(raft_env, reprovision["dut_command"], limit)
    return wait_for_backend(component, raft_env, **wait)


# Used by: L1, L2, and future levels.
def run_reboot(component, raft_env, api, parameters_json=None, *, recovery=None,
               reprovision=None, timeout_seconds=None):
    """run() for an API that may reboot the DUT, then wait until the backend is back.

    A real HAL drops the SSH session while rebooting, so the framed response may never
    arrive. The call therefore:
      1. records the DUT boot id,
      2. issues the API; a RAFT transport/capture failure is tolerated, not trusted,
      3. waits recovery["settle_seconds"], then polls until the backend answers again,
      4. re-reads the boot id. A lost response is accepted only if the boot id changed
         (the DUT really rebooted); otherwise the original failure is re-raised.
    Callers restore state or continue with further tests only after this returns.

    Returns the run() result plus "rebooted" (boot id changed) and "response_lost"
    (no framed response; "output" is "" and "returncode" is None, so the HAL status
    cannot be checked). With the stand-in HAL nothing reboots: the response arrives,
    the backend answers at once and rebooted is False.
    recovery keys (profile "reboot_recovery"): settle_seconds, timeout_seconds,
    poll_interval_seconds.
    reprovision (profile "factory_reset_recovery"; {} enables the hint only) is for
    destructive resets that can wipe network/SSH settings or the dispatcher: if the
    backend is still down after backend_grace_seconds, host_command runs on the test
    host, the DUT is awaited over SSH, dut_command runs on the DUT, and the backend is
    awaited again. If recovery fails the DUT is flagged unrecovered (see
    unrecovered_reason) so later tests are skipped instead of cascading failures.
    """
    cfg = _recovery_settings(recovery)
    if reprovision is not None:
        reprovision = _reprovision_settings(reprovision)
    before = boot_id(raft_env)
    result, lost = None, None
    try:
        result = run(component, raft_env, api, parameters_json,
                     timeout_seconds=timeout_seconds)
    except RuntimeError as exc:
        if not str(exc).startswith("RAFT bridge"):
            raise
        lost = exc
        emit("REBOOT", f"{api}: RAFT session lost ({exc}); waiting for the DUT to recover")
    time.sleep(cfg["settle_seconds"])
    started = perf_counter()
    try:
        probes = _recover_backend(component, raft_env, cfg, reprovision)
    except RuntimeError as exc:
        emit("REBOOT", str(exc), "error", perf_counter() - started)
        _mark_unrecovered(f"DUT did not recover after {api}: {exc}")
        raise
    after = boot_id(raft_env)
    rebooted = before is not None and after is not None and before != after
    if lost is not None and not rebooted:
        emit("REBOOT", f"{api}: session lost but the DUT boot id is unchanged "
             f"(before={before}, after={after}); not a reboot", "error")
        raise lost
    emit("REBOOT", f"{api}: backend responsive after {probes} probe(s) | "
         f"rebooted={rebooted} | response_lost={lost is not None}",
         "complete", perf_counter() - started)
    if lost is not None:
        return {"output": "", "returncode": None, "rebooted": True, "response_lost": True}
    return {**result, "rebooted": rebooted, "response_lost": False}


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
    values = re.findall(rf"(?m)^{re.escape(name)}: ?([^\r\n]*)\r?$", response)
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

        A session that fails before the command is sent (for example the SSH shell was
        not ready) is retried up to BRIDGE_PRESEND_RETRIES times, because nothing ran on
        the DUT. Once the command was sent, failures are never retried.
        """
        for attempt in range(BRIDGE_PRESEND_RETRIES + 1):
            try:
                return self._run_command_once(command, timeout)
            except BridgeNotSentError as exc:
                if attempt == BRIDGE_PRESEND_RETRIES:
                    raise RuntimeError(str(exc)) from None
                emit("RAFT", f"{exc} before the command was sent; retrying "
                     f"({attempt + 1}/{BRIDGE_PRESEND_RETRIES})", "warning")
                time.sleep(BRIDGE_PRESEND_RETRY_DELAY_SECONDS)

    def _run_command_once(self, command, timeout):
        """One bridge session; raises BridgeNotSentError if the command was never sent.

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
                if (error == "transport" and isinstance(transport, dict)
                        and transport.get("command_sent") is not True):
                    raise BridgeNotSentError(f"RAFT bridge failure: {error}")
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