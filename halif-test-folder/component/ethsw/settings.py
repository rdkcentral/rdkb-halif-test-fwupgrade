"""EthSW-owned defaults and registration for the shared controller."""
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

    profile = root / "configs/ethsw_hal.json"
    return Component(
        name="ethsw", profile=profile,
        cli=os.environ.get("ETHSW_VL_DUT_CLI") or str(executable("vl_cli")),
        server=Path(os.environ.get("ETHSW_VL_SERVER") or executable("ethsw_vl")).expanduser(),
        socket_path=os.environ.get("ETHSW_VL_SOCKET_PATH") or "/tmp/ethsw_vl-run.sock",
        log=root / "logs/ethsw/backend.log",
        command_builder=execution.build_command, readiness=execution.responsive,
        startup=execution.startup, preflight=execution.preflight,
        options={"selector": "--ethsw-api", "magic": 0x45535731},
    )
