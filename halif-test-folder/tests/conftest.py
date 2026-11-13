"""Bootstrap shared pytest support and discovered component extensions."""
from framework.configuration import configure, create_settings

_settings = configure(create_settings())
pytest_plugins = ["framework.core"] + [
    component.pytest_plugin for component in _settings.components.values()
    if component.pytest_plugin
]

