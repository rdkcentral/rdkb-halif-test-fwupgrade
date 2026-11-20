"""Controller configuration models, runtime registry, and component discovery."""
from dataclasses import dataclass, field
from importlib import import_module
from pathlib import Path
from typing import Callable


@dataclass(frozen=True)
class Component:
    name: str
    profile: Path
    cli: str
    server: Path
    socket_path: str
    log: Path
    command_builder: Callable
    readiness: Callable
    startup: Callable
    preflight: Callable
    options: dict = field(default_factory=dict)
    profile_adapter: Callable | None = None
    pytest_plugin: str | None = None


@dataclass(frozen=True)
class Settings:
    root: Path
    components: dict
    levels: tuple
    raft_root: Path
    producer: str
    disclaimer: str


_settings = None


def configure(settings):
    global _settings
    if not isinstance(settings, Settings) or not settings.components or not settings.levels:
        raise ValueError("Nonempty controller Settings required")
    if any(name != component.name for name, component in settings.components.items()):
        raise ValueError("Component registry names must match component identities")
    _settings = settings
    return settings


def get_settings():
    if _settings is None:
        raise RuntimeError("Configure controller settings at the project entry point")
    return _settings


ROOT = Path(__file__).resolve().parents[1]


def create_settings():
    """Build Settings from the component registered in configs/settings.py."""
    module = import_module("configs.settings")

    factory = getattr(module, "create_component", None)
    if not callable(factory):
        raise ValueError(
            f"{module.__name__} requires create_component(controller_root)"
        )

    component = factory(ROOT)

    if not isinstance(component, Component):
        raise ValueError(
            f"{module.__name__} must return a Component instance"
        )

    components = {component.name: component}

    return Settings(
        root=ROOT,
        components=components,
        levels=("L1", "L2", "L3"),
        raft_root=ROOT / "third_party/python_raft",
        producer="RDKB HAL Test Suite",
        disclaimer="Mock HAL results do not establish physical hardware behavior.",
    )
