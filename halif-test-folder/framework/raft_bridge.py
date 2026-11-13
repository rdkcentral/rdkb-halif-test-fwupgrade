"""Single-command RAFT bridge for the shared raft_env fixture."""
import json
import os
from pathlib import Path
import re
import shlex
import sys
import uuid
import types


RAFT_ROOT = Path(os.environ["RAFT_BRIDGE_RUNTIME"]).expanduser().resolve()
if not (RAFT_ROOT / "framework/core/testControl.py").is_file():
    raise RuntimeError("Missing configured python_raft runtime")
sys.path.insert(0, str(RAFT_ROOT))
# Both projects use the name framework. This dedicated subprocess must bind it
# to RAFT, even when the controller root is present on PYTHONPATH or sys.path.
raft_package = types.ModuleType("framework")
raft_package.__path__ = [str(RAFT_ROOT / "framework")]
sys.modules["framework"] = raft_package

from framework.core.testControl import testController

# PUBLIC_INTERFACE
class RemoteCommand(testController):
    """Capture one remote command independently of its API exit status."""

    def __init__(self):
        testController.__init__(self, testName="pytest_command", qcId="command-001", maxRunTime=390)
        self.observed_pass = False
        self.transport = {}
        # Already argv-quoted by execution.command(); preserve inline JSON text.
        self.command = os.environ["RAFT_BRIDGE_COMMAND"]
        self.timeout = int(os.environ["RAFT_BRIDGE_TIMEOUT"])
        self.output = ""
        self.returncode = None
        self.error = "transport"

    def constructLogPath(self, rackName, slotName):
        self.logConfig = dict(self.logConfig)
        self.logConfig["directory"] = os.environ["RAFT_BRIDGE_LOG_DIR"]
        return super().constructLogPath(rackName, slotName)

    # PUBLIC_INTERFACE
    def testFunction(self):
        """Return success only for a fully framed output and remote exit code."""
        self.observed_pass = False
        self.output = ""
        self.returncode = None
        self.error = "transport"
        nonce = uuid.uuid4().hex
        begin, end, status = [prefix + nonce for prefix in
                              ("PYTEST_BEGIN_", "PYTEST_END_", "PYTEST_RC_")]
        # Shell concatenation hides contiguous markers from terminal echo.
        split = lambda marker: repr(marker[:-1]) + repr(marker[-1:])
        # Outer quoting protects the full command as the argument to sh -c.
        # The inner argv quoting from execution preserves --json as one argument.
        # Both layers are intentional; do not strip quotes or re-encode JSON.
        command = (f"echo {split(begin)}; sh -c {shlex.quote(self.command)} 2>&1; "
                   f"rc=$?; echo; echo {status}=$rc; echo {split(end)}")
        # Send through the rack-configured RAFT console (SSH in this setup).
        # Framing separates command output/exit status from prompt and echo.
        if not self.session.write(command):
            self.log.stepResult(False, "Could not send pytest command")
            return False
        self.transport["command_sent"] = True
        try:
            response = self._text(self.session.read_until(end, timeout=self.timeout))
        except TimeoutError:
            self.error = "timeout"
            self.log.stepResult(False, "Remote command timed out")
            return False
        if not isinstance(response, str):
            self.error = "malformed_capture"
            self.log.stepResult(False, "Remote command capture was not text")
            return False
        normalized = response.replace("\r\n", "\n")
        # Interactive Bash can disable bracketed paste and return the cursor
        # before printing the begin marker. Accept only that framing prefix;
        # do not strip terminal controls or lone carriage returns from output.
        match = re.search(r"(?m)^(?:\x1b\[\?2004l)?\r?" +
                          re.escape(begin) + r"\n(.*?)\n" +
                          re.escape(status) + r"=(\d+)\n" + re.escape(end) + r"(?:\n|$)",
                          normalized, re.DOTALL)
        self.error = ("malformed_capture" if re.search(
            r"(?m)^" + re.escape(end) + r"(?:\n|$)", normalized
        ) else "incomplete_capture")
        if match and 0 <= int(match.group(2)) <= 255:
            self.output = match.group(1)
            self.returncode = int(match.group(2))
            self.observed_pass = True
            self.transport["capture_completed"] = True
            self.error = None
        self.log.stepResult(self.observed_pass, "Remote command capture completed")
        return self.observed_pass

    @staticmethod
    def _text(output):
        """Normalize remote console output."""
        if isinstance(output, bytes):
            return output.decode("utf-8", errors="replace")
        return output

    # PUBLIC_INTERFACE
    def testPrepareFunction(self):
        """Prepare the SSH shell before executing the requested command."""
        self.session.prompt = "RAFT_COMMAND> "

        try:
            if not self.session.write("export PS1='RAFT_COMMAND> '"):
                self.log.stepResult(False, "Could not write to SSH shell")
                return False

            output = self._text(
                self.session.read_until(self.session.prompt, timeout=10)
            )

            if not output.endswith(self.session.prompt):
                output += self._text(
                    self.session.read_until(self.session.prompt, timeout=10)
                )

            ready = output.endswith(self.session.prompt)
            self.transport["shell_ready"] = ready
            self.log.stepResult(ready, "SSH shell ready")
            return ready

        except Exception:
            self.log.stepResult(False, "SSH shell preparation failed")
            return False


# PUBLIC_INTERFACE
def main():
    """Write a structured result; process success means capture, not API success."""
    test = None
    passed = False
    error = "transport"
    try:
        test = RemoteCommand()
        passed = test.run(powerOff=False) is True and test.observed_pass
        error = None if passed else (test.error or "raft_failure")
    except TimeoutError:
        error = "timeout"
        print("RAFT bridge timed out; inspect RAFT_BRIDGE_LOG_DIR logs", file=sys.stderr)
    except Exception:
        print("RAFT bridge failed; inspect RAFT_BRIDGE_LOG_DIR logs", file=sys.stderr)
    finally:
        session = getattr(test, "session", None)
        if session is not None:
            try:
                session.close()
                test.transport["session_close_completed"] = True
            except Exception:
                test.transport["session_close_completed"] = False
        Path(os.environ["RAFT_BRIDGE_RESULT"]).write_text(json.dumps({
            "passed": bool(passed), "completed": bool(passed), "error": error,
            "returncode": getattr(test, "returncode", None),
            "output": getattr(test, "output", ""),
            "transport": getattr(test, "transport", {})}))
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

