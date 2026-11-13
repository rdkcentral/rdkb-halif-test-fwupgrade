"""FWUpgrade-owned defaults and registration for the shared controller."""
import os
from pathlib import Path
from framework.configuration import Component
from framework import execution


def create_component(controller_root):
    root = Path(controller_root)
    dut_root = root

    def executable(filename):
        for directory in (dut_root / "build-run", dut_root / "install-run/bin"):
            candidate = directory / filename
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
        return dut_root / "build-run" / filename

    profile = root / "configs/fwupgrade_hal.json"
    return Component(
        name="fwupgrade", profile=profile,
        cli=os.environ.get("FWUPGRADE_VL_DUT_CLI") or str(executable("vl_cli")),
        server=Path(os.environ.get("FWUPGRADE_VL_SERVER") or executable("fwupgrade_vl")).expanduser(),
        socket_path=os.environ.get("FWUPGRADE_VL_SOCKET_PATH") or "/tmp/fwupgrade_vl-run.sock",
        log=root / "logs/fwupgrade/backend.log",
        command_builder=execution.build_command, readiness=execution.responsive,
        startup=execution.startup, preflight=execution.preflight,
        options={"selector": "--fwupgrade-api", "magic": 0x464c5631},
    )

