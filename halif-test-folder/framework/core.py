"""Settings-driven selection and structural JSON/YAML profile loading."""
import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import pytest

from .configuration import get_settings

SELECTION_OPTIONS = ("--component", "--level", "--api", "--valid", "--invalid", "--case-id")


def positive_integer(value):
    try:
        number = int(value)
    except (TypeError, ValueError):
        raise argparse.ArgumentTypeError("Use a positive integer") from None
    if number < 1:
        raise argparse.ArgumentTypeError("Use a positive integer")
    return number


def add_selection_options(add):
    COMPONENTS = get_settings().components
    LEVELS = get_settings().levels
    add("--component", nargs="+", action="extend", default=None,
        choices=[*COMPONENTS, "all"],
        help="Component or all (default)")
    add("--level", nargs="+", action="extend", default=None,
        choices=[*LEVELS, "all"], help=f"Levels (default: {LEVELS[0]}); all selects available levels")
    add("--api", default=None, help="Exact API/profile section name")
    add("--valid", action="store_true", default=False, help="Select only valid cases")
    add("--invalid", action="store_true", default=False, help="Select only invalid cases")
    add("--case-id", type=positive_integer, default=None, help="Exact positive profile Case_ID")


def _expand(values, available, default):
    values = list(values) if values is not None else list(default)
    if "all" in values:
        if len(values) != 1:
            raise ValueError("Use all alone, not with other selections")
        return tuple(available)
    unknown = set(values) - set(available)
    if unknown:
        raise ValueError("Unknown selection: " + ", ".join(sorted(unknown)))
    return tuple(dict.fromkeys(values))


@dataclass(frozen=True)
class Selection:
    components: tuple
    levels: tuple
    api: str | None
    case_type: str | None
    case_id: int | None

    @property
    def has_case_filters(self):
        return self.api is not None or self.case_type is not None or self.case_id is not None

    def test_paths(self, root=None):
        root = Path(root) if root is not None else get_settings().root
        return (root / "tests",)

    def pytest_args(self):
        # Equals-form prevents nargs from swallowing a subsequent test path.
        args = [f"--component={name}" for name in self.components]
        args += [f"--level={level}" for level in self.levels]
        if self.api is not None:
            args.append(f"--api={self.api}")
        if self.case_type is not None:
            args.append(f"--{self.case_type}")
        if self.case_id is not None:
            args.append(f"--case-id={self.case_id}")
        return args


def resolve_selection(options):
    """Normalize filters before backend startup or collection."""
    settings = get_settings()
    COMPONENTS, LEVELS = settings.components, settings.levels
    components = _expand(options.get("component"), COMPONENTS, ("all",))
    levels = _expand(options.get("level"), LEVELS, (LEVELS[0],))
    if options.get("valid") and options.get("invalid"):
        raise ValueError("--valid and --invalid are mutually exclusive; omit both to run both categories")
    case_type = "valid" if options.get("valid") else "invalid" if options.get("invalid") else None
    api, case_id = options.get("api"), options.get("case_id")
    if api is not None and not api.strip():
        raise ValueError("--api must be a nonempty exact API name")
    if case_id is not None:
        if type(case_id) is not int or case_id < 1:
            raise ValueError("--case-id must be a positive integer")
        if len(components) != 1 or len(levels) != 1 or api is None or case_type is None:
            raise ValueError("--case-id requires one component, one level, --api, and --valid or --invalid")
    return Selection(components, levels, api, case_type, case_id)


def level_of(node):
    markers = [marker for marker in node.iter_markers()
               if marker.name in get_settings().levels]
    if len(markers) != 1 or markers[0].args or markers[0].kwargs:
        raise pytest.UsageError(
            f"{node.nodeid}: declare exactly one level marker from "
            + ", ".join(get_settings().levels)
        )
    return markers[0].name


# Numbered profile loading and parametrization
def _mapping(value, label):
    """Validate a declaration mapping without imposing API value rules."""
    if (not isinstance(value, dict) or not value
            or any(not isinstance(k, str) or not k.isidentifier()
                   or not isinstance(v, str) or not v for k, v in value.items())
            or len(set(value.values())) != len(value)):
        raise pytest.UsageError(f"profile {label} must be a nonempty unique mapping")
    return value


def profile_parameters(profile, declaration):
    """Flatten generic groups into argument names and pytest parameters."""
    allowed = {"section", "groups", "inputs", "expected", "id_labels"}
    if set(declaration) - allowed:
        raise pytest.UsageError("Unknown profile marker option")
    section_name = declaration.get("section")
    if not isinstance(section_name, str) or not section_name:
        raise pytest.UsageError("profile section must be a nonempty name")
    groups = declaration.get("groups")
    if (not isinstance(groups, (list, tuple)) or not groups
            or any(not isinstance(g, str) or g not in ("valid", "invalid")
                   for g in groups)
            or len(set(groups)) != len(groups)):
        raise pytest.UsageError("profile groups must be unique valid/invalid names")
    inputs = _mapping(declaration.get("inputs"), "inputs")
    # Input-only declarations may omit expectations.
    expected = (_mapping(declaration["expected"], "expected")
                if "expected" in declaration else {})
    names = ["case_type", *inputs, *expected]
    if len(set(names)) != len(names):
        raise pytest.UsageError("profile argument mappings overlap")
    labels = declaration.get("id_labels", {name: name for name in inputs})
    if (not isinstance(labels, dict) or set(labels) != set(inputs)
            or any(not isinstance(v, str) or not v for v in labels.values())):
        raise pytest.UsageError("profile id_labels must name every input")
    section = profile.get(section_name)
    if not isinstance(section, dict):
        raise pytest.UsageError(f"Profile requires section {section_name}")
    if set(section) - {"columns", "valid", "invalid"}:
        raise pytest.UsageError(f"{section_name}: unsupported group or key")
    columns = section.get("columns")
    if "columns" in section:
        if (not isinstance(columns, list) or not columns
                or any(not isinstance(c, str) or not c for c in columns)
                or len(set(columns)) != len(columns)):
            raise pytest.UsageError(f"{section_name}: columns must be unique names")
        if set(inputs.values()) != set(columns):
            raise pytest.UsageError(f"{section_name}: input mappings must match columns")
    parameters, seen_ids = [], set()
    for group_name in groups:
        group = section.get(group_name)
        if not isinstance(group, dict):
            raise pytest.UsageError(f"{section_name}.{group_name}: missing group mapping")
        if any(key not in group for key in expected.values()):
            raise pytest.UsageError(f"{section_name}.{group_name}: missing expected key")
        case_numbers = None
        if columns is not None:
            cases = group.get("cases")
            # A profile category may contain an empty cases list.
            if not isinstance(cases, list):
                raise pytest.UsageError(f"{section_name}.{group_name}: cases list required")
            rows, case_numbers, seen_numbers = [], [], set()
            for case in cases:
                label = f"{section_name}.{group_name}"
                if not isinstance(case, dict) or set(case) != {"Case_ID", "parameters"}:
                    raise pytest.UsageError(f"{label}: each case requires Case_ID and parameters only")
                number = case["Case_ID"]
                if type(number) is not int or number < 1:
                    raise pytest.UsageError(f"{label}: Case_ID must be a positive integer")
                if number in seen_numbers:
                    raise pytest.UsageError(f"{label}: duplicate Case_ID {number}")
                seen_numbers.add(number)
                parameters_by_name = case["parameters"]
                if (not isinstance(parameters_by_name, dict)
                        or set(parameters_by_name) != set(columns)):
                    raise pytest.UsageError(f"{label}: parameters must match columns exactly")
                rows.append([parameters_by_name[column] for column in columns])
                case_numbers.append(number)
            values = [[row[columns.index(source)] for source in inputs.values()]
                      for row in rows]
        else:
            lists = [group.get(source) for source in inputs.values()]
            if (any(not isinstance(items, list) or not items for items in lists)
                    or len({len(items) for items in lists}) != 1):
                raise pytest.UsageError(
                    f"{section_name}.{group_name}: nonempty equal-length input lists required"
                )
            values = list(zip(*lists))
        for index, row in enumerate(values):
            parts = [section_name, group_name]
            if case_numbers is not None:
                parts.append(f"TC{case_numbers[index]:03d}")
            for name, value in zip(inputs, row):
                parts.extend((labels[name], str(value)))
            base = re.sub(r"[^a-zA-Z0-9_.-]+", "_", "-".join(parts))
            case_id = base
            suffix = 2
            while case_id in seen_ids:
                case_id = f"{base}-case-{suffix}"
                suffix += 1
            seen_ids.add(case_id)
            parameters.append(pytest.param(
                group_name, *row, *(group[key] for key in expected.values()),
                id=case_id,
                marks=pytest.mark.hal_case(
                    api=section_name, case_type=group_name,
                    case_id=case_numbers[index] if case_numbers is not None else None,
                ),
            ))
    return names, parameters


def load_profile(path, cache, adapter=None):
    path = Path(path).expanduser().resolve()
    key = (path, adapter)
    if key not in cache:
        try:
            text = path.read_text(encoding="utf-8")
            if path.suffix.lower() in (".yml", ".yaml"):
                value = load_yaml(text)
            else:
                value = json.loads(text)
        except (OSError, UnicodeError, ValueError) as error:
            raise pytest.UsageError(f"Cannot load profile {path}: {type(error).__name__}") from None
        if not isinstance(value, dict):
            raise pytest.UsageError("Profile must be a mapping")
        value = adapter(value) if adapter else value
        if not isinstance(value, dict):
            raise pytest.UsageError("Adapted profile must be a mapping")
        cache[key] = value
    return cache[key]


def load_yaml(text):
    try:
        import yaml
    except ImportError:
        raise pytest.UsageError("YAML profiles require PyYAML") from None
    try:
        return yaml.safe_load(text)
    except yaml.YAMLError:
        raise pytest.UsageError("Invalid profile YAML") from None


# Shared pytest hooks and fixtures, loaded by the controller bootstrap.
def pytest_addoption(parser):
    add_selection_options(parser.addoption)
    parser.addoption("--profile", default=None, help="Override the selected component test profile")
    for name in get_settings().components:
        parser.addoption(f"--{name}-profile", default=None)

def pytest_configure(config):
    try:
        config._hal_selection = resolve_selection(vars(config.option))
    except ValueError as error:
        raise pytest.UsageError(str(error)) from None
    selected = config._hal_selection.components
    if config.getoption("profile") and len(selected) != 1:
        raise pytest.UsageError("--profile requires exactly one selected component")
    if len(selected) == 1 and config.getoption("profile") and config.getoption(f"{selected[0]}_profile"):
        raise pytest.UsageError("Do not combine --profile and a component-specific override")


def component_of(node):
    markers = list(node.iter_markers(name="component"))
    if (len(markers) != 1 or len(markers[0].args) != 1 or markers[0].kwargs
            or not isinstance(markers[0].args[0], str)
            or markers[0].args[0] not in get_settings().components):
        raise pytest.UsageError(
            f"{node.nodeid}: declare exactly one @pytest.mark.component(name) "
            "with a registered component"
        )
    return markers[0].args[0]

def selected_profile(config, component):
    path = config.getoption(f"{component}_profile") or config.getoption("profile") or get_settings().components[component].profile
    path = Path(path).expanduser()
    if not path.is_absolute():
        path = get_settings().root / path
    if not hasattr(config, "_profiles_by_path"):
        config._profiles_by_path = {}
    return load_profile(path, config._profiles_by_path, get_settings().components[component].profile_adapter)

@pytest.fixture
def raft_env(request):
    config = os.environ.get("RAFT_CONFIG")
    if not config:
        pytest.skip("Set RAFT_CONFIG to enable RAFT DUT tests")
    from .execution import RaftEnvironment
    return RaftEnvironment(config, component_of(request.node))

@pytest.fixture
def api_name(request):
    markers = list(request.node.iter_markers(name="profile"))
    if len(markers) != 1 or markers[0].args:
        raise pytest.UsageError("Use exactly one keyword-only profile marker per test")
    section = markers[0].kwargs.get("section")
    if not isinstance(section, str) or not isinstance(selected_profile(request.config, component_of(request.node)).get(section), dict):
        raise pytest.UsageError("Profile requires API section")
    return section

def pytest_generate_tests(metafunc):
    markers = list(metafunc.definition.iter_markers(name="profile"))
    if not markers:
        return
    if len(markers) != 1 or markers[0].args:
        raise pytest.UsageError("Use exactly one keyword-only profile marker per test")
    component = component_of(metafunc.definition)
    selected = metafunc.config._hal_selection
    level = level_of(metafunc.definition)
    declaration = markers[0].kwargs
    # Do not load profiles for unselected components, levels or APIs.
    if (component not in selected.components or level not in selected.levels
            or (selected.api is not None and declaration.get("section") != selected.api)):
        names = ["case_type", *declaration.get("inputs", {}), *declaration.get("expected", {})]
        metafunc.parametrize(names, [])
        return
    names, parameters = profile_parameters(selected_profile(metafunc.config, component), declaration)
    if set(names) - set(metafunc.fixturenames):
        raise pytest.UsageError("Profile mappings must match testcase arguments")
    metafunc.parametrize(names, parameters)

def pytest_collection_modifyitems(config, items):
    selected = config._hal_selection
    kept, deselected = [], []
    for item in items:
        component, level = component_of(item), level_of(item)
        matches = component in selected.components and level in selected.levels
        if matches and selected.has_case_filters:
            case = item.get_closest_marker("hal_case")
            matches = case is not None and all(
                wanted is None or case.kwargs.get(key) == wanted
                for key, wanted in (("api", selected.api), ("case_type", selected.case_type),
                                    ("case_id", selected.case_id))
            )
        (kept if matches else deselected).append(item)
    if deselected:
        config.hook.pytest_deselected(items=deselected)
        items[:] = kept
    if not any(item.get_closest_marker("raft") for item in items):
        return
    if (getattr(config, "workerinput", None) is not None or
            config.getoption("numprocesses", default=0) not in (None, 0, "0")):
        raise pytest.UsageError("RAFT state-changing tests must run serially; use -n 0")

