"""Platform-owned defaults and registration for the shared controller."""
import os
from pathlib import Path
from framework.configuration import Component
from framework import execution


def create_component(controller_root):
    root = Path(controller_root)

    def executable(filename):
        for directory in (root / "build-run", root / "install-run/bin"):
            candidate = directory / filename
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
        return root / "build-run" / filename

    return Component(
        name="platform", profile=root / "configs/platform_hal.json",
        cli=os.environ.get("PLATFORM_VL_DUT_CLI") or str(executable("vl_cli")),
        server=Path(os.environ.get("PLATFORM_VL_SERVER") or executable("platform_vl")).expanduser(),
        socket_path=os.environ.get("PLATFORM_VL_SOCKET_PATH") or "/tmp/platform_vl-run.sock",
        log=root / "logs/platform/backend.log",
        command_builder=execution.build_command, readiness=execution.responsive,
        startup=execution.startup, preflight=execution.preflight,
        options={"selector": "--platform-api", "magic": 0x50564c31},
    )
