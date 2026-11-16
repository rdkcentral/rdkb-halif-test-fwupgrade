"""Platform L1: common static contracts and profile-driven device cases.

Telnet/SSH/timeout/LED/SNMP SET restores state. LED checks are not visual.
"""
from contextlib import contextmanager
import json
import re

import pytest

from framework.execution import assert_equal, field, restore_fields, run

pytestmark = [pytest.mark.raft, pytest.mark.component("platform"), pytest.mark.L1]


def string_field(response, name):
    """Read exactly one string field from CLI output."""
    values = re.findall(
        rf'^{re.escape(name)}:[ \t]*([^\r\n]*)\r?$',
        response,
        flags=re.MULTILINE,
    )
    assert len(values) == 1, (
        f"Expected exactly one string {name} field"
    )
    return values[0]


@pytest.mark.feature(name='telnet')
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param('valid', 'DEFAULT', 0, marks=pytest.mark.hal_case(
            api='platform_hal_GetTelnetEnable', case_type='valid', case_id=1,
        )),
        pytest.param('invalid', 'NULL', -1, marks=pytest.mark.hal_case(
            api='platform_hal_GetTelnetEnable', case_type='invalid', case_id=1,
        )),
    ],
)
def test_01_platform_get_telnet_enable(raft_env, case_type, output_pointer, expected_status):
    result = run(
        "platform", raft_env, "platform_hal_GetTelnetEnable",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    status = field(response, "status")
    assert_equal(
        status, expected_status, "API status",
        f"{case_type}: output_pointer={output_pointer!r}; "
        f"expected status={expected_status}, got {status}",
    )


@pytest.mark.feature(name='telnet')
@pytest.mark.parametrize(
    "case_type, flag, expected_status",
    [
        pytest.param('valid', 1, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetTelnetEnable', case_type='valid', case_id=1,
        )),
        pytest.param('valid', 0, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetTelnetEnable', case_type='valid', case_id=2,
        )),
    ],
)
def test_02_platform_set_telnet_enable(raft_env, case_type, flag, expected_status):
    with restore_fields(
        "platform", raft_env,
        get_api="platform_hal_GetTelnetEnable",
        set_api="platform_hal_SetTelnetEnable",
        fields={"flag": "value"},
        restore_as_json=True,
    ):
        payload = json.dumps({"flag": flag}, separators=(",", ":"))
        result = run(
            "platform", raft_env, "platform_hal_SetTelnetEnable",
            parameters_json=payload,
        )
        response = result["output"]
        status = field(response, "status")
        assert_equal(
            status, expected_status, "API status",
            f"{case_type}: flag={flag!r}; "
            f"expected status={expected_status}, got {status}",
        )


@pytest.mark.feature(name='led')
@pytest.mark.profile(
    section="platform_hal_getLed",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_03_platform_get_led(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate LED GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    # These cases use a valid buffer or a HAL-rejected NULL output pointer.
    expected_returncode = 0 if case_type == "valid" else 1
    context = f"{case_type}: output_pointer={output_pointer!r}; result={result}"
    assert_equal(
        field(result["output"], "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )

# ---------------------------------------------------------------------------
# LED SET: inputs are generated from the selected device profile
# ---------------------------------------------------------------------------

@pytest.mark.feature(name='led')
@pytest.mark.profile(
    section="platform_hal_setLed",
    groups=("valid", "invalid"),
    inputs={"p_value": "pValue"},
    expected={"expected_status": "expected_status"},
)
def test_04_platform_set_led(
    raft_env, api_name, case_type, p_value, expected_status,
):
    """Check reported LED SET status for structured and NULL inputs.

    A pValue object supplies LedColor, State, and Interval to the adapter.
    JSON null maps to an actual HAL NULL input pointer.
    Representable LED fields are forwarded for HAL semantic validation.
    """
    if p_value is not None:
        assert isinstance(p_value, dict), "pValue must be an object or null"
        assert set(p_value) == {"LedColor", "State", "Interval"}, (
            "pValue must contain exactly LedColor, State, and Interval"
        )
    # run() invokes command(), which quotes --json as one CLI argument.
    # RAFT sends the request inline; no request JSON file is uploaded.
    with restore_fields(
        "platform", raft_env,
        get_api="platform_hal_getLed", set_api=api_name,
        fields={"LedColor": "LedColor", "State": "State", "Interval": "Interval"},
        restore_as_json=True,
    ):
        if p_value is None:
            result = run(
                "platform", raft_env, api_name,
                parameters={"pValue": "NULL"},
            )
        else:
            result = run(
                "platform", raft_env, api_name,
                parameters_json=json.dumps(p_value, separators=(",", ":")),
            )
        actual_status = field(result["output"], "status")
        assert_equal(actual_status, expected_status, "API status", (
            f"{case_type}: pValue={p_value!r}; "
            f"expected status={expected_status}; "
            f"actual status={actual_status}; result={result}"
        ))


# ---------------------------------------------------------------------------
# Static SSH and timeout cases declare selection metadata with each row.
# ---------------------------------------------------------------------------

@pytest.mark.feature(name='ssh')
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param('valid', 'DEFAULT', 0, marks=pytest.mark.hal_case(
            api='platform_hal_GetSSHEnable',
            case_type='valid',
            case_id=1,
        )),
        pytest.param('invalid', 'NULL', -1, marks=pytest.mark.hal_case(
            api='platform_hal_GetSSHEnable',
            case_type='invalid',
            case_id=1,
        )),
    ],
)
def test_05_platform_get_ssh_enable(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetSSHEnable",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )

    assert_equal(
        field(result["output"], "status"),
        expected_status,
        "GET API status",
        f"{case_type}: output_pointer={output_pointer!r}",
    )

    assert_equal(
        result["returncode"],
        0 if case_type == "valid" else 1,
        "CLI exit",
        f"{case_type}: output_pointer={output_pointer!r}",
    )


@pytest.mark.feature(name='ssh')
@pytest.mark.parametrize(
    "case_type, flag, expected_status",
    [
        pytest.param("valid", 1, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetSSHEnable', case_type='valid', case_id=1,
        )),
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetSSHEnable', case_type='valid', case_id=2,
        )),
    ],
)
def test_06_platform_set_ssh_enable(raft_env, case_type, flag, expected_status):
    with restore_fields(
        "platform", raft_env,
        get_api="platform_hal_GetSSHEnable",
        set_api="platform_hal_SetSSHEnable",
        fields={"flag": "value"},
        restore_as_json=True,
    ):
        result = run(
            "platform", raft_env, "platform_hal_SetSSHEnable",
            parameters_json=json.dumps({"flag": flag}, separators=(",", ":")),
        )
        assert_equal(
            field(result["output"], "status"), expected_status, "SET API status",
            f"{case_type}: flag={flag!r}",
        )


@pytest.mark.feature(name='webui')
@pytest.mark.parametrize(
    "case_type, expected_status",
    [
        pytest.param("valid", 0, marks=pytest.mark.hal_case(
            api='platform_hal_GetWebUITimeout', case_type='valid', case_id=1,
        )),
    ],
)
def test_07_platform_get_webui_timeout(raft_env, case_type, expected_status):
    result = run("platform", raft_env, "platform_hal_GetWebUITimeout")
    assert_equal(
        field(result["output"], "status"), expected_status, "GET API status", case_type,
    )


@pytest.mark.feature(name='webui')
@pytest.mark.parametrize(
    "case_type, value, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetWebUITimeout', case_type='valid', case_id=1,
        )),
        pytest.param("valid", 30, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetWebUITimeout', case_type='valid', case_id=2,
        )),
        pytest.param("valid", 86400, 0, marks=pytest.mark.hal_case(
            api='platform_hal_SetWebUITimeout', case_type='valid', case_id=3,
        )),
        pytest.param("invalid", 29, -1, marks=pytest.mark.hal_case(
            api='platform_hal_SetWebUITimeout', case_type='invalid', case_id=1,
        )),
        pytest.param("invalid", 86401, -1, marks=pytest.mark.hal_case(
            api='platform_hal_SetWebUITimeout', case_type='invalid', case_id=2,
        )),
    ],
)
def test_08_platform_set_webui_timeout(raft_env, case_type, value, expected_status):
    """Check timeout JSON SET status and restore the original state using JSON."""
    with restore_fields(
        "platform", raft_env,
        get_api="platform_hal_GetWebUITimeout", set_api="platform_hal_SetWebUITimeout",
        fields={"value": "value"},
        restore_as_json=True,
    ):
        result = run(
            "platform", raft_env, "platform_hal_SetWebUITimeout",
            parameters_json=json.dumps({"value": value}, separators=(",", ":")),
        )
        assert_equal(
            field(result["output"], "status"), expected_status, "SET API status",
            f"{case_type}: value={value!r}",
        )
        assert_equal(
            result["returncode"], 0 if case_type == "valid" else 1,
            "CLI exit (boundary errors must reach HAL)",
            f"{case_type}: value={value!r}",
        )


# Profile-driven SNMP status cases; SET restores the original state after each case.

@pytest.mark.feature(name="snmp")
@pytest.mark.profile(
    section="platform_hal_GetSNMPEnable",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_09_platform_get_snmp_enable(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Check profile-driven SNMP GET status and NULL-output rejection."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    actual_status = field(result["output"], "status")
    assert_equal(
        actual_status, expected_status, "GET API status",
        f"{case_type}: output_pointer={output_pointer!r}; "
        f"expected status={expected_status}, got {actual_status}",
    )


@pytest.mark.feature(name="snmp")
@pytest.mark.profile(
    section="platform_hal_SetSNMPEnable",
    groups=("valid", "invalid"),
    inputs={"value": "value"},
    expected={"expected_status": "expected_status"},
)
def test_10_platform_set_snmp_enable(
    raft_env, api_name, case_type, value, expected_status,
):
    """Use JSON strings, named NULL input, and restore the original mode."""
    with restore_fields(
        "platform", raft_env,
        get_api="platform_hal_GetSNMPEnable",
        set_api=api_name,
        fields={},
        text_fields={"pvalue": "value"},
        restore_as_json=True,
    ):
        if value == "NULL":
            result = run(
                "platform", raft_env, api_name,
                parameters={"pvalue": "NULL"},
            )
        else:
            assert isinstance(value, str), "SNMP value must be a string"
            result = run(
                "platform", raft_env, api_name,
                parameters_json=json.dumps(
                    {"pvalue": value}, separators=(",", ":"),
                ),
            )
        actual_status = field(result["output"], "status")
        assert_equal(
            actual_status, expected_status, "SET API status",
            f"{case_type}: pvalue={value!r}; "
            f"expected status={expected_status}, got {actual_status}",
        )


@pytest.mark.feature(name="model_name")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_GetModelName", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_GetModelName", case_type="invalid", case_id=1,
        )),
    ],
)
def test_11_platform_get_model_name(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform", raft_env, "platform_hal_GetModelName",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()), True,
            "Device model name is nonempty", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No model-name value on failure", context,
        )


@pytest.mark.feature(name="router_region")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetRouterRegion",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetRouterRegion",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_12_platform_get_router_region(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetRouterRegion",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Router region is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No router-region value on failure",
            context,
        )


@pytest.mark.feature(name="serial_number")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetSerialNumber",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetSerialNumber",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_13_platform_get_serial_number(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetSerialNumber",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Device serial number is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No serial-number value on failure",
            context,
        )


@pytest.mark.feature(name="hardware_version")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardwareVersion",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardwareVersion",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_14_platform_get_hardware_version(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetHardwareVersion",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Hardware version is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No hardware-version value on failure",
            context,
        )


@pytest.mark.feature(name="software_version")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetSoftwareVersion",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetSoftwareVersion",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_15_platform_get_software_version(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetSoftwareVersion",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Software version is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No software-version value on failure",
            context,
        )


@pytest.mark.feature(name="bootloader_version")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetBootloaderVersion",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetBootloaderVersion",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_16_platform_get_bootloader_version(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetBootloaderVersion",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Bootloader version is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No bootloader-version value on failure",
            context,
        )


@pytest.mark.feature(name="firmware_name")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetFirmwareName",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetFirmwareName",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_17_platform_get_firmware_name(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetFirmwareName",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            bool(string_field(response, "value").strip()),
            True,
            "Firmware image name is nonempty",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No firmware-name value on failure",
            context,
        )


@pytest.mark.feature(name="base_mac_address")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetBaseMacAddress",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetBaseMacAddress",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_18_platform_get_base_mac_address(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetBaseMacAddress",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        octets = string_field(response, "value").split(":")
        assert_equal(
            len(octets) == 6 and all(
                len(octet) == 2
                and all(char in "0123456789abcdefABCDEF" for char in octet)
                for octet in octets
            ),
            True,
            "Base MAC address has six hexadecimal octets",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No base-MAC-address value on failure",
            context,
        )


@pytest.mark.feature(name="hardware")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_19_platform_get_hardware(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetHardware",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            1 <= field(response, "value") <= 4096,
            True,
            "Total flash size is within the documented 1..4096 MB",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No flash-size value on failure",
            context,
        )


@pytest.mark.feature(name="hardware_mem_used")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware_MemUsed",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware_MemUsed",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_20_platform_get_hardware_mem_used(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetHardware_MemUsed",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            1 <= field(response, "value") <= 4096,
            True,
            "Used flash size is positive and within the flash capacity limit",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No used-flash value on failure",
            context,
        )


@pytest.mark.feature(name="hardware_mem_free")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware_MemFree",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetHardware_MemFree",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_21_platform_get_hardware_mem_free(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetHardware_MemFree",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            1 <= field(response, "value") <= 4096,
            True,
            "Available flash size is positive and within the flash capacity limit",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No available-flash value on failure",
            context,
        )


@pytest.mark.feature(name="total_memory_size")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetTotalMemorySize",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetTotalMemorySize",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_22_platform_get_total_memory_size(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetTotalMemorySize",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            field(response, "value") >= 1,
            True,
            "Total DRAM size is positive in MB",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No total-DRAM value on failure",
            context,
        )


@pytest.mark.feature(name="used_memory_size")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetUsedMemorySize",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetUsedMemorySize",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_23_platform_get_used_memory_size(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetUsedMemorySize",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            field(response, "value") >= 1,
            True,
            "Used DRAM size is positive in MB",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No used-DRAM value on failure",
            context,
        )


@pytest.mark.feature(name="free_memory_size")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetFreeMemorySize",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetFreeMemorySize",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_24_platform_get_free_memory_size(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetFreeMemorySize",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            field(response, "value") >= 1,
            True,
            "Available DRAM size is positive in MB",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No available-DRAM value on failure",
            context,
        )


@pytest.mark.feature(name="factory_reset_count")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_GetFactoryResetCount", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_GetFactoryResetCount", case_type="invalid", case_id=1,
        )),
    ],
)
def test_25_platform_get_factory_reset_count(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform", raft_env, "platform_hal_GetFactoryResetCount",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type == "valid":
        assert_equal(
            field(response, "value") >= 0, True,
            "Factory-reset count is nonnegative", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No factory-reset count on failure", context,
        )


@pytest.mark.feature(name="factory_reset_count")
@pytest.mark.parametrize(
    "case_type, flag, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_ClearResetCount", case_type="valid", case_id=1,
        )),
        pytest.param("valid", 1, 0, marks=pytest.mark.hal_case(
            api="platform_hal_ClearResetCount", case_type="valid", case_id=2,
        )),
    ],
)
def test_26_platform_clear_reset_count(raft_env, case_type, flag, expected_status):
    """TRUE clears the counter; no available HAL API can restore it."""
    context = f"{case_type}: flag={flag!r}"
    before = run("platform", raft_env, "platform_hal_GetFactoryResetCount")
    assert_equal(
        field(before["output"], "status"), 0, "Initial GET API status", context,
    )
    original_count = field(before["output"], "value")

    # No restore_fields: TRUE permanently clears a real-device counter.
    result = run(
        "platform", raft_env, "platform_hal_ClearResetCount",
        parameters_json=json.dumps({"flag": flag}, separators=(",", ":")),
    )
    assert_equal(
        field(result["output"], "status"), expected_status, "SET API status", context,
    )

    # Readback assumes no concurrent factory-reset counter modifications.
    after = run("platform", raft_env, "platform_hal_GetFactoryResetCount")
    assert_equal(
        field(after["output"], "status"), 0, "Readback GET API status", context,
    )
    expected_count = 0 if case_type == "valid" and flag == 1 else original_count
    assert_equal(
        field(after["output"], "value"), expected_count,
        "Factory-reset count after clear request", context,
    )


@pytest.mark.feature(name="time_offset")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_getTimeOffSet", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_getTimeOffSet", case_type="invalid", case_id=1,
        )),
    ],
)
def test_27_platform_get_time_offset(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform", raft_env, "platform_hal_getTimeOffSet",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type == "valid":
        offset = string_field(response, "value")
        digits = offset[1:] if offset[:1] in ("+", "-") else offset
        assert_equal(
            digits.isascii() and digits.isdigit(),
            True, "Time offset is an integer in seconds", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No time-offset value on failure", context,
        )


@pytest.mark.feature(name="device_code_image_timeout")
@pytest.mark.parametrize(
    "case_type, seconds, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageTimeout",
            case_type="valid",
            case_id=1,
        )),
        pytest.param("valid", 3600, 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageTimeout",
            case_type="valid",
            case_id=2,
        )),
        pytest.param("invalid", 3601, -1, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageTimeout",
            case_type="invalid",
            case_id=1,
        )),
        pytest.param("invalid", -1, -1, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageTimeout",
            case_type="invalid",
            case_id=2,
        )),
    ],
)
def test_28_platform_set_device_code_image_timeout(
    raft_env, case_type, seconds, expected_status,
):
    """Check JSON SET status; no matching HAL getter permits restoration."""
    result = run(
        "platform",
        raft_env,
        "platform_hal_SetDeviceCodeImageTimeout",
        parameters_json=json.dumps({"seconds": seconds}, separators=(",", ":")),
    )

    assert_equal(
        field(result["output"], "status"),
        expected_status,
        "SET API status",
        f"{case_type}: seconds={seconds!r}",
    )
    assert_equal(
        result["returncode"], 0 if case_type == "valid" else 1,
        "CLI exit (boundary errors must reach HAL)",
        f"{case_type}: seconds={seconds!r}",
    )


@pytest.mark.feature(name="device_code_image_valid")
@pytest.mark.parametrize(
    "case_type, flag, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageValid",
            case_type="valid",
            case_id=1,
        )),
        pytest.param("valid", 1, 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetDeviceCodeImageValid",
            case_type="valid",
            case_id=2,
        )),
    ],
)
def test_29_platform_set_device_code_image_valid(
    raft_env, case_type, flag, expected_status,
):
    """Check JSON SET status; no matching HAL getter permits restoration."""
    result = run(
        "platform",
        raft_env,
        "platform_hal_SetDeviceCodeImageValid",
        parameters_json=json.dumps({"value": flag}, separators=(",", ":")),
    )

    assert_equal(
        field(result["output"], "status"),
        expected_status,
        "SET API status",
        f"{case_type}: flag={flag!r}",
    )


@pytest.mark.feature(name="factory_partner_id")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFactoryPartnerId",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFactoryPartnerId",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_30_platform_get_factory_partner_id(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_getFactoryPartnerId",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            string_field(response, "value") in (
                "unknown", "eUnprogrammed", "eComcast", "eCharter", "eCox",
                "eRogers", "eVodafone", "eShaw", "eVideotron",
            ),
            True,
            "Factory partner ID follows the HAL contract",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No factory-partner ID on failure",
            context,
        )


@pytest.mark.feature(name="factory_cm_variant")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFactoryCmVariant",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFactoryCmVariant",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_31_platform_get_factory_cm_variant(
    raft_env, case_type, output_pointer, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_getFactoryCmVariant",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        assert_equal(
            string_field(response, "value") in (
                "unknown", "pc20", "pc20genband", "pc15sip", "pc15mgcp",
            ),
            True,
            "Factory CM variant follows the HAL contract",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No factory-CM variant on failure",
            context,
        )


@pytest.mark.feature(name="factory_cm_variant")
@pytest.mark.parametrize(
    "case_type, value, expected_status",
    [
        pytest.param("valid", "unknown", 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="valid",
            case_id=1,
        )),
        pytest.param("valid", "pc20", 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="valid",
            case_id=2,
        )),
        pytest.param("valid", "pc20genband", 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="valid",
            case_id=3,
        )),
        pytest.param("valid", "pc15sip", 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="valid",
            case_id=4,
        )),
        pytest.param("valid", "pc15mgcp", 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="valid",
            case_id=5,
        )),
        pytest.param("invalid", "invalidVariant", -1, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="invalid",
            case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_setFactoryCmVariant",
            case_type="invalid",
            case_id=2,
        )),
    ],
)
def test_32_platform_set_factory_cm_variant(
    raft_env, case_type, value, expected_status,
):
    """Check JSON string/named NULL SET status without readback or restoration."""
    context = f"{case_type}: value={value!r}"

    if value == "NULL":
        result = run(
            "platform", raft_env, "platform_hal_setFactoryCmVariant",
            parameters={"value": "NULL"},
        )
    else:
        result = run(
            "platform", raft_env, "platform_hal_setFactoryCmVariant",
            parameters_json=json.dumps({"value": value}, separators=(",", ":")),
        )
    assert_equal(
        field(result["output"], "status"),
        expected_status,
        "SET API status",
        context,
    )


@pytest.mark.feature(name="fan_speed")
@pytest.mark.parametrize(
    "case_type, fan_index, expected_status",
    [
        pytest.param(
            "valid", 0, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFanSpeed",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "valid", 1, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getFanSpeed",
                case_type="valid",
                case_id=2,
            ),
        ),
    ],
)
def test_33_platform_get_fan_speed(
    raft_env, case_type, fan_index, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_getFanSpeed",
        parameters={"fanIndex": fan_index},
    )
    response = result["output"]
    context = f"{case_type}: fanIndex={fan_index!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "Command status",
        context,
    )

    if case_type == "valid":
        # Units and maximum speed are OEM-dependent.
        assert_equal(
            field(response, "value") >= 0,
            True,
            "Fan speed is nonnegative",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No fan-speed value on rejected request",
            context,
        )


@pytest.mark.feature(name="fan_rpm")
@pytest.mark.parametrize(
    "case_type, fan_index, expected_status",
    [
        pytest.param(
            "valid", 0, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getRPM",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "valid", 1, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getRPM",
                case_type="valid",
                case_id=2,
            ),
        ),
    ],
)
def test_34_platform_get_rpm(
    raft_env, case_type, fan_index, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_getRPM",
        parameters={"fanIndex": fan_index},
    )
    response = result["output"]
    context = f"{case_type}: fanIndex={fan_index!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "Command status",
        context,
    )

    if case_type == "valid":
        # Maximum RPM is OEM-dependent.
        assert_equal(
            field(response, "value") >= 0,
            True,
            "Fan RPM is nonnegative",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No RPM value on rejected request",
            context,
        )


@pytest.mark.feature(name="fan_rotor_lock")
@pytest.mark.parametrize(
    "case_type, fan_index, expected_status",
    [
        pytest.param(
            "valid", 0, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getRotorLock",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "valid", 1, 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_getRotorLock",
                case_type="valid",
                case_id=2,
            ),
        ),
    ],
)
def test_35_platform_get_rotor_lock(
    raft_env, case_type, fan_index, expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_getRotorLock",
        parameters={"fanIndex": fan_index},
    )
    response = result["output"]
    context = f"{case_type}: fanIndex={fan_index!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "Command status",
        context,
    )

    if case_type == "valid":
        lock = field(response, "value")
        assert_equal(
            lock in (-1, 0, 1),
            True,
            "Rotor-lock reading follows the HAL contract",
            context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False,
            "No rotor-lock value on rejected request",
            context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="fan_status")
@pytest.mark.parametrize(
    "case_type, fan_index, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_getFanStatus", case_type="valid", case_id=1,
        )),
        pytest.param("valid", 1, 0, marks=pytest.mark.hal_case(
            api="platform_hal_getFanStatus", case_type="valid", case_id=2,
        )),
    ],
)
def test_36_platform_get_fan_status(
    raft_env, case_type, fan_index, expected_status,
):
    """Check Boolean fan status and scalar-input rejection."""
    result = run(
        "platform", raft_env, "platform_hal_getFanStatus",
        parameters={"fanIndex": fan_index},
    )
    response = result["output"]
    context = f"{case_type}: fanIndex={fan_index!r}; result={result}"
    # The HAL returns a Boolean reading; status belongs to the command adapter.
    # NULL is an invalid scalar token, not a pointer passed to the HAL.
    assert_equal(
        field(response, "status"), expected_status, "Command status", context,
    )
    assert_equal(
        result["returncode"], 0 if case_type == "valid" else 2,
        "CLI exit", context,
    )
    if case_type == "valid":
        value = field(response, "value")
        assert_equal(
            value in (0, 1), True, "Fan status is Boolean", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No fan-status value on rejected request", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="fan_max_override")
@pytest.mark.parametrize(
    "case_type, override_flag, fan_index, expected_status",
    [
        pytest.param("valid", 0, 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="valid", case_id=1,
        )),
        pytest.param("valid", 1, 0, 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="valid", case_id=2,
        )),
        pytest.param("valid", 0, 1, 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="valid", case_id=3,
        )),
        pytest.param("valid", 1, 1, 0, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="valid", case_id=4,
        )),
        pytest.param("invalid", 0, 2, -1, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="invalid", case_id=4,
        )),
        pytest.param("invalid", 1, 2, -1, marks=pytest.mark.hal_case(
            api="platform_hal_setFanMaxOverride", case_type="invalid", case_id=3,
        )),
    ],
)
def test_37_platform_set_fan_max_override(
    raft_env, case_type, override_flag, fan_index, expected_status,
):
    """Validate one JSON override SET without GET readback or cleanup."""
    context = (
        f"{case_type}: bOverrideFlag={override_flag!r}, fanIndex={fan_index!r}"
    )
    # Invalid flags are adapter-rejected; index 2 reaches HAL validation.
    expected_returncode = 0 if case_type == "valid" else (
        1 if override_flag in (0, 1) and fan_index == 2 else 2
    )
    result = run(
        "platform", raft_env, "platform_hal_setFanMaxOverride",
        parameters_json=json.dumps(
            {"bOverrideFlag": override_flag, "fanIndex": fan_index},
            separators=(",", ":"),
        ),
    )
    assert_equal(
        field(result["output"], "status"), expected_status,
        "Command status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode,
        "CLI exit", context,
    )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="fan_temperature")
@pytest.mark.profile(
    section="platform_hal_getFanTemperature",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_38_platform_get_fan_temperature(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate fan-temperature GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    # NULL reaches the HAL; unsupported tokens fail before HAL dispatch.
    expected_returncode = 0 if case_type == "valid" else (
        1 if output_pointer == "NULL" else 2
    )
    context = f"{case_type}: output_pointer={output_pointer!r}; result={result}"
    assert_equal(
        field(result["output"], "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )
    if case_type == "valid":
        # Parse exactly one signed integer; temperatures may be below zero.
        field(result["output"], "value")
    else:
        assert_equal(
            any(line.startswith("value:") for line in result["output"].splitlines()),
            False, "No fan-temperature value on failure", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="input_power")
@pytest.mark.profile(
    section="platform_hal_getInputPower",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_39_platform_get_input_power(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate input-power GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    # NULL reaches the HAL; unsupported tokens fail before HAL dispatch.
    expected_returncode = 0 if case_type == "valid" else (
        1 if output_pointer == "NULL" else 2
    )
    context = f"{case_type}: output_pointer={output_pointer!r}; result={result}"
    assert_equal(
        field(result["output"], "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )
    if case_type == "valid":
        assert_equal(
            field(result["output"], "value") >= 0, True,
            "Input power is nonnegative (mW)", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in result["output"].splitlines()),
            False, "No input-power value on failure", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="input_current")
@pytest.mark.profile(
    section="platform_hal_getInputCurrent",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_40_platform_get_input_current(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate input-current GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    # NULL reaches the HAL; unsupported tokens fail before HAL dispatch.
    expected_returncode = 0 if case_type == "valid" else (
        1 if output_pointer == "NULL" else 2
    )
    context = f"{case_type}: output_pointer={output_pointer!r}; result={result}"
    assert_equal(
        field(result["output"], "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )
    if case_type == "valid":
        assert_equal(
            field(result["output"], "value") >= 0, True,
            "Input current is nonnegative (mA)", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in result["output"].splitlines()),
            False, "No input-current value on failure", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="radio_temperature")
@pytest.mark.profile(
    section="platform_hal_getRadioTemperature",
    groups=("valid", "invalid"),
    inputs={"radio_index": "radioIndex", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_41_platform_get_radio_temperature(
    raft_env, api_name, case_type, radio_index, output_pointer, expected_status,
):
    """Validate radio-temperature GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        parameters={"radioIndex": radio_index},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    # Invalid index 3 and NULL output pointers are forwarded to HAL.
    expected_returncode = 0 if case_type == "valid" else 1
    context = (
        f"{api_name}: {case_type}: radioIndex={radio_index!r}, "
        f"output_pointer={output_pointer!r}; result={result}"
    )
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )
    if case_type == "valid":
        value = field(response, "value")
        assert_equal(
            0 <= value <= 100, True, "Radio temperature is within 0..100", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No radio-temperature value on rejected request", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="eco_mode_status")
@pytest.mark.profile(
    section="platform_hal_getEcoModeStatus",
    groups=("valid", "invalid"),
    inputs={"radio_index": "radioIndex", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_42_platform_get_eco_mode_status(
    raft_env, api_name, case_type, radio_index, output_pointer, expected_status,
):
    """Validate ECO-mode status GET using cases from the selected JSON profile."""
    result = run(
        "platform", raft_env, api_name,
        parameters={"radioIndex": radio_index},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    # Profile inputs are representable INT indexes with supported pointer
    # selectors. Invalid indexes and NULL output pointers must reach the HAL.
    expected_returncode = 0 if case_type == "valid" else 1
    context = (
        f"{api_name}: {case_type}: radioIndex={radio_index!r}, "
        f"output_pointer={output_pointer!r}; result={result}"
    )
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )
    assert_equal(
        result["returncode"], expected_returncode, "CLI exit", context,
    )
    if case_type == "valid":
        value = field(response, "value")
        assert_equal(
            value in (0, 1), True, "ECO-mode status is Boolean", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No ECO-mode status value on rejected request", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name='snmp')
@pytest.mark.parametrize(
    "case_type, value, expected_status",
    [
        pytest.param("valid", "enable", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=1,
        )),
        pytest.param("valid", "disable", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=2,
        )),
        pytest.param("valid", "ENABLE", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=3,
        )),
        pytest.param("valid", "DISABLE", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=4,
        )),
        pytest.param("valid", "EnAbLe", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=5,
        )),
        pytest.param("valid", "DiSaBlE", 0, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="valid", case_id=6,
        )),
        pytest.param("invalid", "invalid", -1, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="invalid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_SetSNMPOnboardRebootEnable",
            case_type="invalid", case_id=2,
        )),
    ],
)
def test_43_platform_set_snmp_onboard_reboot_enable(
    raft_env, case_type, value, expected_status,
):
    """Check case-insensitive reboot-enable modes and invalid string rejection.

    Literal "NULL" is forwarded as a string, not converted to a C NULL pointer.
    No matching getter is exposed; GetSNMPEnable reads a different setting.
    Each case sends one SET without readback or cleanup.
    """
    api = "platform_hal_SetSNMPOnboardRebootEnable"
    context = f"{case_type}: pvalue={value!r}"
    result = run(
        "platform", raft_env, api,
        parameters_json=json.dumps({"pvalue": value}, separators=(",", ":")),
    )
    response = result["output"]
    assert_equal(
        field(response, "status"), expected_status, "SET API status", context,
    )
    assert_equal(
        any(line.startswith("value:") for line in response.splitlines()),
        False, "No value returned by setter", context,
    )


def _macsec_snapshot(raft_env, eth_port):
    """Read only the requested port; inaccessible ports have no mutable setup."""
    values = []
    for api in ("platform_hal_GetMACsecEnable",
                "platform_hal_GetMACsecOperationalStatus"):
        response = run(
            "platform", raft_env, api, parameters={"ethPort": eth_port},
        )["output"]
        if field(response, "status") != 0:
            return None
        value = field(response, "value")
        assert_equal(value in (0, 1), True, "MACsec Boolean", api)
        values.append(value)
    return tuple(values)


def _macsec_configure(raft_env, eth_port, state):
    """Configure and verify one accessible port, including during restoration."""
    enabled, running = state
    commands = [
        ("platform_hal_StopMACsec", {"ethPort": eth_port}),
        ("platform_hal_SetMACsecEnable", {"ethPort": eth_port, "Flag": enabled}),
    ]
    if running:
        commands.append((
            "platform_hal_StartMACsec", {"ethPort": eth_port, "timeoutSec": 0},
        ))
    for api, parameters in commands:
        response = run("platform", raft_env, api, parameters=parameters)["output"]
        assert_equal(field(response, "status"), 0, "MACsec setup/restore", api)
    assert_equal(
        _macsec_snapshot(raft_env, eth_port), state, "MACsec setup/restore readback",
        f"ethPort={eth_port}",
    )


@contextmanager
def _macsec_state(raft_env, eth_port, baseline):
    """Set up only the requested accessible port, without restoring its state."""
    if _macsec_snapshot(raft_env, eth_port) is None:
        yield False
        return
    _macsec_configure(raft_env, eth_port, baseline)
    # Deliberately leave the resulting state in place, including on failure.
    yield True


def _macsec_getter_case(raft_env, api, eth_port, output_pointer, expected_status,
                        baseline):
    """Assert getter status/value and unchanged baseline, without restoration."""
    with _macsec_state(raft_env, eth_port, baseline) as accessible:
        response = run(
            "platform", raft_env, api, parameters={"ethPort": eth_port},
            output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
        )["output"]
        context = f"{api}: ethPort={eth_port}, output_pointer={output_pointer}"
        assert_equal(field(response, "status"), expected_status, "API status", context)
        if expected_status == 0:
            assert_equal(field(response, "value"), 1, "MACsec getter value", context)
        else:
            assert_equal(
                any(line.startswith("value:") for line in response.splitlines()),
                False, "No value on rejected getter", context,
            )
        if accessible:
            assert_equal(
                _macsec_snapshot(raft_env, eth_port), baseline,
                "Getter leaves state unchanged", context,
            )


def _macsec_mutation_case(raft_env, api, parameters, expected_status, baseline,
                          success_state):
    """Check mutation/readback or rejection without checking CLI exit codes."""
    eth_port = parameters["ethPort"]
    with _macsec_state(raft_env, eth_port, baseline) as accessible:
        response = run("platform", raft_env, api, parameters=parameters)["output"]
        context = f"{api}: {parameters}"
        assert_equal(field(response, "status"), expected_status, "API status", context)
        if accessible:
            expected = success_state if expected_status == 0 else baseline
            assert_equal(
                _macsec_snapshot(raft_env, eth_port), expected,
                "MACsec state after request", context,
            )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="macsec")
@pytest.mark.profile(
    section="platform_hal_GetMACsecEnable", groups=["valid", "invalid"],
    inputs={"eth_port": "ethPort", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_44_platform_get_macsec_enable(
    raft_env, case_type, eth_port, output_pointer, expected_status,
):
    """Read enable status without setup or restoration."""
    response = run(
        "platform",
        raft_env,
        "platform_hal_GetMACsecEnable",
        parameters={"ethPort": eth_port},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )["output"]
    context = (
        f"{case_type}: ethPort={eth_port}, "
        f"output_pointer={output_pointer}"
    )
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )
    if expected_status == 0:
        assert_equal(
            field(response, "value") in (0, 1),
            True, "MACsec Boolean value", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No value on rejected getter", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="macsec")
@pytest.mark.profile(
    section="platform_hal_GetMACsecOperationalStatus", groups=["valid", "invalid"],
    inputs={"eth_port": "ethPort", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_45_platform_get_macsec_operational_status(
    raft_env, case_type, eth_port, output_pointer, expected_status,
):
    """Read operational status without setup or restoration."""
    response = run(
        "platform",
        raft_env,
        "platform_hal_GetMACsecOperationalStatus",
        parameters={"ethPort": eth_port},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )["output"]
    context = (
        f"{case_type}: ethPort={eth_port}, "
        f"output_pointer={output_pointer}"
    )
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )
    if expected_status == 0:
        assert_equal(
            field(response, "value") in (0, 1),
            True, "MACsec Boolean value", context,
        )
    else:
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No value on rejected getter", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="macsec")
@pytest.mark.profile(
    section="platform_hal_SetMACsecEnable", groups=["valid", "invalid"],
    inputs={"eth_port": "ethPort", "flag": "Flag"},
    expected={"expected_status": "expected_status"},
)
def test_46_platform_set_macsec_enable(
    raft_env, case_type, eth_port, flag, expected_status,
):
    """Set enable status using JSON without setup, readback, or restoration."""
    response = run(
        "platform",
        raft_env,
        "platform_hal_SetMACsecEnable",
        parameters_json=json.dumps(
            {"ethPort": eth_port, "Flag": flag}, separators=(",", ":"),
        ),
    )["output"]
    context = f"{case_type}: ethPort={eth_port}, Flag={flag}"
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="macsec")
@pytest.mark.profile(
    section="platform_hal_StartMACsec", groups=["valid", "invalid"],
    inputs={"eth_port": "ethPort", "timeout_sec": "timeoutSec"},
    expected={"expected_status": "expected_status"},
)
def test_47_platform_start_macsec(
    raft_env, case_type, eth_port, timeout_sec, expected_status,
):
    """Start MACsec using JSON without setup, readback, or restoration."""
    response = run(
        "platform",
        raft_env,
        "platform_hal_StartMACsec",
        parameters_json=json.dumps(
            {"ethPort": eth_port, "timeoutSec": timeout_sec}, separators=(",", ":"),
        ),
    )["output"]
    context = f"{case_type}: ethPort={eth_port}, timeoutSec={timeout_sec}"
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="macsec")
@pytest.mark.profile(
    section="platform_hal_StopMACsec", groups=["valid", "invalid"],
    inputs={"eth_port": "ethPort"},
    expected={"expected_status": "expected_status"},
)
def test_48_platform_stop_macsec(raft_env, case_type, eth_port, expected_status):
    """Stop MACsec using JSON without setup, readback, or restoration."""
    response = run(
        "platform",
        raft_env,
        "platform_hal_StopMACsec",
        parameters_json=json.dumps({"ethPort": eth_port}, separators=(",", ":")),
    )["output"]
    context = f"{case_type}: ethPort={eth_port}"
    assert_equal(
        field(response, "status"), expected_status, "API status", context,
    )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="low_power_mode")
@pytest.mark.profile(
    section="platform_hal_SetLowPowerModeState",
    groups=("valid", "invalid"),
    inputs={"state": "state"},
    expected={"expected_status": "expected_status"},
)
def test_49_platform_set_low_power_mode_state(
    raft_env, api_name, case_type, state, expected_status,
):
    """Check low-power JSON SET status using the selected isolated-mock profile.

    No matching HAL getter exists for readback or restoration. Named NULL
    exercises a local C NULL input pointer; JSON states must be integers.
    These cases do not validate hardware power management.
    """
    if state == "NULL":
        result = run(
            "platform", raft_env, api_name, parameters={"state": "NULL"},
        )
    else:
        result = run(
            "platform", raft_env, api_name,
            parameters_json=json.dumps({"state": state}, separators=(",", ":")),
        )
    assert_equal(
        field(result["output"], "status"), expected_status, "SET API status",
        f"{api_name}: {case_type}: state={state!r}; result={result}",
    )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="ppp")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_GetPppUserName", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_GetPppUserName", case_type="invalid", case_id=1,
        )),
    ],
)
def test_50_platform_get_ppp_username(
    raft_env, case_type, output_pointer, expected_status,
):
    """Check API status and actual HAL NULL-output rejection."""
    result = run(
        "platform", raft_env, "platform_hal_GetPppUserName",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type != "valid":
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No PPP username on failure", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="ppp")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_GetPppPassword", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_GetPppPassword", case_type="invalid", case_id=1,
        )),
    ],
)
def test_51_platform_get_ppp_password(
    raft_env, case_type, output_pointer, expected_status,
):
    """Check API status; use synthetic credentials because responses are logged."""
    result = run(
        "platform", raft_env, "platform_hal_GetPppPassword",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type != "valid":
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No PPP password on failure", context,
        )


# PUBLIC_INTERFACE
@pytest.mark.feature(name="cpu_speed")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="platform_hal_GetCPUSpeed", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="platform_hal_GetCPUSpeed", case_type="invalid", case_id=1,
        )),
    ],
)
def test_52_platform_get_cpu_speed(
    raft_env, case_type, output_pointer, expected_status,
):
    """Check API status for a valid CPU speed buffer and an actual NULL pointer."""
    result = run(
        "platform", raft_env, "platform_hal_GetCPUSpeed",
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    status = field(result["output"], "status")
    assert_equal(
        status, expected_status, "API status",
        f"{case_type}: output_pointer={output_pointer!r}; "
        f"expected status={expected_status}, got {status}",
    )


@pytest.mark.feature(name="database_init")
@pytest.mark.hal_case(
    api="platform_hal_PandMDBInit", case_type="valid", case_id=1,
)
def test_53_platform_pandm_db_init(raft_env):
    """Mock/approved DUT only: real initialization may modify persistent state.

    Checks reported success, not database contents or repeat-call behavior.
    """
    api_name = "platform_hal_PandMDBInit"
    result = run("platform", raft_env, api_name)
    response = result["output"]
    context = f"{api_name}: no input parameters"
    assert_equal(
        field(response, "status"), 0, "PandM initialization status", context,
    )
    assert_equal(result["returncode"], 0, "Successful CLI exit", context)
    assert_equal(
        any(line.startswith("error:") for line in response.splitlines()),
        False, "No initialization error", context,
    )


@pytest.mark.feature(name="database_init")
@pytest.mark.hal_case(
    api="platform_hal_DocsisParamsDBInit", case_type="valid", case_id=1,
)
def test_54_platform_docsis_params_db_init(raft_env):
    """Mock/approved supported DUT only: initialization may modify DB state.

    Checks reported success, not database contents or repeat-call behavior.
    """
    api_name = "platform_hal_DocsisParamsDBInit"
    result = run("platform", raft_env, api_name)
    response = result["output"]
    context = f"{api_name}: no input parameters"
    assert_equal(
        field(response, "status"), 0, "DOCSIS initialization status", context,
    )
    assert_equal(result["returncode"], 0, "Successful CLI exit", context)
    assert_equal(
        any(line.startswith("error:") for line in response.splitlines()),
        False, "No initialization error", context,
    )


def _check_empty_dhcp_options(
    raft_env,
    api,
    case_type,
    req_pointer,
    send_pointer,
    expected_status,
):
    parameters = {}
    if req_pointer != "DEFAULT":
        parameters["req-pointer"] = req_pointer
    if send_pointer != "DEFAULT":
        parameters["send-pointer"] = send_pointer

    result = run("platform", raft_env, api, parameters=parameters)
    response = result["output"]
    context = (
        f"{api}: {case_type}, "
        f"req_pointer={req_pointer!r}, "
        f"send_pointer={send_pointer!r}"
    )
    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        # Intentionally checks the empty-list mock, not real DHCP options.
        assert_equal(
            field(response, "req_count"),
            0,
            "Empty request-option list",
            context,
        )
        assert_equal(
            field(response, "send_count"),
            0,
            "Empty send-option list",
            context,
        )
    else:
        for name in ("req_count", "send_count"):
            assert_equal(
                any(
                    line.startswith(f"{name}:")
                    for line in response.splitlines()
                ),
                False,
                f"No {name} on failure",
                context,
            )


@pytest.mark.feature(name="dhcpv6_options")
@pytest.mark.parametrize(
    "case_type, req_pointer, send_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv6_Options",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv6_Options",
                case_type="invalid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "DEFAULT", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv6_Options",
                case_type="invalid",
                case_id=2,
            ),
        ),
        pytest.param(
            "invalid", "NULL", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv6_Options",
                case_type="invalid",
                case_id=3,
            ),
        ),
    ],
)
def test_55_platform_get_dhcpv6_options(
    raft_env,
    case_type,
    req_pointer,
    send_pointer,
    expected_status,
):
    """Empty-list mock contract, not real DHCP option validation."""
    _check_empty_dhcp_options(
        raft_env,
        "platform_hal_GetDhcpv6_Options",
        case_type,
        req_pointer,
        send_pointer,
        expected_status,
    )


@pytest.mark.feature(name="dhcpv4_options")
@pytest.mark.parametrize(
    "case_type, req_pointer, send_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv4_Options",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv4_Options",
                case_type="invalid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "DEFAULT", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv4_Options",
                case_type="invalid",
                case_id=2,
            ),
        ),
        pytest.param(
            "invalid", "NULL", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDhcpv4_Options",
                case_type="invalid",
                case_id=3,
            ),
        ),
    ],
)
def test_56_platform_get_dhcpv4_options(
    raft_env,
    case_type,
    req_pointer,
    send_pointer,
    expected_status,
):
    """Empty-list mock contract, not real DHCP option validation."""
    _check_empty_dhcp_options(
        raft_env,
        "platform_hal_GetDhcpv4_Options",
        case_type,
        req_pointer,
        send_pointer,
        expected_status,
    )


@pytest.mark.feature(name="device_config_status")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param(
            "valid", "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDeviceConfigStatus",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetDeviceConfigStatus",
                case_type="invalid",
                case_id=1,
            ),
        ),
    ],
)
def test_57_platform_get_device_config_status(
    raft_env,
    case_type,
    output_pointer,
    expected_status,
):
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetDeviceConfigStatus",
        output_pointer=(
            None if output_pointer == "DEFAULT" else output_pointer
        ),
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        value = string_field(response, "value")
        assert value in {
            "WaitForImplement",
            "In Progress",
            "Complete",
        }, f"{context}: unexpected configuration status {value!r}"
    else:
        assert_equal(
            any(
                line.startswith("value:")
                for line in response.splitlines()
            ),
            False,
            "No configuration-status value on failure",
            context,
        )


@pytest.mark.feature(name="memory_paths")
@pytest.mark.parametrize(
    "case_type, index, output_pointer, expected_status",
    [
        pytest.param(
            "valid", 0, "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="valid",
                case_id=1,
            ),
        ),
        pytest.param(
            "valid", 1, "DEFAULT", 0,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="valid",
                case_id=2,
            ),
        ),
        pytest.param(
            "invalid", 0, "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=1,
            ),
        ),
        pytest.param(
            "invalid", 1, "NULL", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=2,
            ),
        ),
        pytest.param(
            "invalid", -1, "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=3,
            ),
        ),
        pytest.param(
            "invalid", 2, "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=4,
            ),
        ),
        pytest.param(
            "invalid", 3, "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=5,
            ),
        ),
        pytest.param(
            "invalid", 4, "DEFAULT", -1,
            marks=pytest.mark.hal_case(
                api="platform_hal_GetMemoryPaths",
                case_type="invalid",
                case_id=6,
            ),
        ),
    ],
)
def test_58_platform_get_memory_paths(
    raft_env,
    case_type,
    index,
    output_pointer,
    expected_status,
):
    """Current mock supports HOST_CPU and PEER_CPU."""
    result = run(
        "platform",
        raft_env,
        "platform_hal_GetMemoryPaths",
        parameters={"index": index},
        output_pointer=(
            None if output_pointer == "DEFAULT" else output_pointer
        ),
    )
    response = result["output"]
    context = (
        f"{case_type}: index={index}, "
        f"output_pointer={output_pointer!r}"
    )
    path_fields = (
        "dramPath",
        "emmcPath1",
        "emmcPath2",
        "emmcPath3",
    )

    assert_equal(
        field(response, "status"),
        expected_status,
        "GET API status",
        context,
    )

    if case_type == "valid":
        for name in path_fields:
            assert_equal(
                any(
                    line.startswith(f"{name}:")
                    for line in response.splitlines()
                ),
                True,
                f"Memory path field {name} present",
                context,
            )

            value = string_field(response, name)

            # Header defines a 40-byte buffer, including termination.
            assert len(value.encode("utf-8")) < 40, (
                f"{context}: {name} exceeds its buffer size"
            )

            # Unused eMMC paths may be empty.
            assert not value or value.startswith("/"), (
                f"{context}: invalid path {name}={value!r}"
            )

        assert string_field(response, "dramPath"), (
            f"{context}: DRAM path is empty"
        )
    else:
        for name in path_fields:
            assert_equal(
                any(
                    line.startswith(f"{name}:")
                    for line in response.splitlines()
                ),
                False,
                f"No {name} on failure",
                context,
            )


@pytest.mark.feature(name="fan_speed")
@pytest.mark.profile(
    section="platform_hal_setFanSpeed",
    groups=("valid", "invalid"),
    inputs={
        "fan_index": "fanIndex",
        "fan_speed": "fanSpeed",
        "output_pointer": "output_pointer",
    },
    expected={"expected_status": "expected_status"},
)
def test_59_platform_set_fan_speed(
    raft_env, api_name, case_type, fan_index, fan_speed,
    output_pointer, expected_status,
):
    """Validate profile cases, including actual HAL NULL rejection.

    Default-output cases use JSON; explicit pointer cases use named inputs.
    Requires override disabled on both fans; changes speeds without restoration.
    Use the mock backend, not an unapproved real-device thermal test.
    """
    parameters = {"fanIndex": fan_index, "fanSpeed": fan_speed}
    if output_pointer == "DEFAULT":
        result = run(
            "platform", raft_env, api_name,
            parameters_json=json.dumps(parameters, separators=(",", ":")),
        )
    else:
        result = run(
            "platform", raft_env, api_name, parameters=parameters,
            output_pointer=output_pointer,
        )
    response = result["output"]
    context = (
        f"{case_type}: fanIndex={fan_index!r}, fanSpeed={fan_speed!r}, "
        f"output_pointer={output_pointer!r}"
    )
    assert_equal(
        field(response, "status"), expected_status, "SET API status", context,
    )
    has_reason = any(line.startswith("reason:") for line in response.splitlines())
    has_error = any(line.startswith("error:") for line in response.splitlines())
    if fan_index not in (0, 1):
        assert_equal(result["returncode"], 2, "Invalid-argument CLI exit", context)
        assert_equal(
            string_field(response, "error"), "invalid argument value",
            "Adapter validation error", context,
        )
        assert_equal(has_reason, False, "No HAL reason on rejected input", context)
    elif fan_speed not in (0, 1, 2, 3, 4) or output_pointer == "NULL":
        assert_equal(result["returncode"], 1, "HAL-failure CLI exit", context)
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "HAL rejection error", context,
        )
        assert_equal(
            has_reason, output_pointer != "NULL",
            "HAL reason only through a non-NULL pointer", context,
        )
    else:
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        assert_equal(field(response, "reason"), 0, "FAN_ERR_NONE", context)
        assert_equal(has_error, False, "No error on successful SET", context)


@pytest.mark.feature(name="cmts_mac")
@pytest.mark.profile(
    section="platform_hal_getCMTSMac",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_60_platform_get_cmts_mac(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate CMTS MAC format and actual HAL NULL-pointer rejection."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status, "CMTS MAC API status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        mac = string_field(response, "value")
        octets = mac.split(":")
        valid_mac = len(octets) == 6 and all(
            len(octet) == 2
            and all(char in "0123456789abcdefABCDEF" for char in octet)
            for octet in octets
        )
        assert_equal(
            valid_mac, True, "CMTS MAC uses six hexadecimal octets", context,
        )
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful CMTS MAC retrieval", context,
        )
    else:
        assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        assert_equal(result["returncode"], 1, "HAL-failure CLI exit", context)
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "NULL-pointer rejection", context,
        )
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No MAC value on failure", context,
        )


@pytest.mark.feature(name="led")
@pytest.mark.profile(
    section="platform_hal_initLed",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_61_platform_init_led(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate LED filename output and actual HAL NULL-pointer rejection."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status,
        "LED initialization API status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        config_file_name = string_field(response, "config_file_name")
        assert_equal(
            bool(config_file_name.strip()), True,
            "Configuration filename is nonempty", context,
        )
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful LED initialization", context,
        )
    else:
        assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        assert_equal(result["returncode"], 1, "HAL-failure CLI exit", context)
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "NULL-pointer rejection", context,
        )
        assert_equal(
            any(line.startswith("config_file_name:") for line in response.splitlines()),
            False, "No configuration filename on failure", context,
        )


@pytest.mark.feature(name="thermal")
@pytest.mark.profile(
    section="platform_hal_LoadThermalConfig",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_62_platform_load_thermal_config(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate thermal configuration output and actual HAL NULL rejection."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    config_fields = (
        "FanCount", "SlowSpeedThresh", "MediumSpeedThresh",
        "FastSpeedThresh", "FanMinRunTime", "MonitoringDelay",
        "PowerMonitoring", "LogInterval",
    )
    assert_equal(
        field(response, "status"), expected_status,
        "Load thermal configuration status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful configuration load", context,
        )
        config = {name: field(response, name) for name in config_fields}
        assert_equal(
            config["FanCount"] in (1, 2), True,
            "FanCount follows the header contract", context,
        )
        for name in config_fields:
            assert_equal(
                config[name] >= 0, True, f"{name} is nonnegative", context,
            )
    else:
        assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        assert_equal(result["returncode"], 1, "HAL-failure CLI exit", context)
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "NULL-pointer rejection", context,
        )
        for name in config_fields:
            assert_equal(
                any(line.startswith(f"{name}:") for line in response.splitlines()),
                False, f"No {name} output on failure", context,
            )


@pytest.mark.feature(name="thermal")
@pytest.mark.profile(
    section="platform_hal_initThermal",
    groups=("valid", "invalid"),
    inputs={"output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_63_platform_init_thermal(
    raft_env, api_name, case_type, output_pointer, expected_status,
):
    """Validate thermal initialization using two dynamic profile cases."""
    result = run(
        "platform", raft_env, api_name,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = f"{case_type}: output_pointer={output_pointer!r}"
    assert_equal(
        field(response, "status"), expected_status,
        "Thermal initialization status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful initialization", context,
        )
    else:
        assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        assert_equal(result["returncode"], 1, "HAL-failure CLI exit", context)
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "NULL-pointer rejection", context,
        )
        config_fields = (
            "FanCount", "SlowSpeedThresh", "MediumSpeedThresh",
            "FastSpeedThresh", "FanMinRunTime", "MonitoringDelay",
            "PowerMonitoring", "LogInterval",
        )
        for name in config_fields:
            assert_equal(
                any(line.startswith(f"{name}:") for line in response.splitlines()),
                False, f"No {name} output on failure", context,
            )


def _assert_dscp_result(result, expected_status, expected_exit, context):
    """Check shared API status, CLI exit, and error fields."""
    response = result["output"]
    assert_equal(
        field(response, "status"), expected_status, "DSCP API status", context,
    )
    assert_equal(
        result["returncode"], expected_exit, "DSCP CLI exit", context,
    )
    if expected_exit == 0:
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on success", context,
        )
    else:
        assert_equal(
            string_field(response, "error"),
            "invalid argument value" if expected_exit == 2 else "HAL operation failed",
            "DSCP failure error", context,
        )


@pytest.mark.feature(name="dscp")
@pytest.mark.profile(
    section="platform_hal_setDscp",
    groups=("valid", "invalid"),
    inputs={
        "interface_type": "interfaceType",
        "cmd": "cmd",
        "dscp_values": "pDscpVals",
    },
    expected={"expected_status": "expected_status"},
)
def test_64_platform_set_dscp(
    raft_env, api_name, case_type, interface_type, cmd,
    dscp_values, expected_status,
):
    """Dedicated DUT/mock only; STOP cleanup cannot restore prior configuration.

    Run serially. Profile null omits the JSON key; literal "NULL" remains text.
    """
    parameters = {"interfaceType": interface_type, "cmd": cmd}
    if dscp_values is not None:
        parameters["pDscpVals"] = dscp_values
    context = (
        f"{case_type}: interfaceType={interface_type!r}, "
        f"cmd={cmd!r}, pDscpVals={dscp_values!r}"
    )
    try:
        result = run(
            "platform", raft_env, api_name,
            parameters_json=json.dumps(parameters, separators=(",", ":")),
        )
        _assert_dscp_result(
            result, expected_status, 0 if case_type == "valid" else 1, context,
        )
    finally:
        if case_type == "valid" and cmd == 1:
            stopped = run(
                "platform", raft_env, api_name,
                parameters_json=json.dumps(
                    {"interfaceType": interface_type, "cmd": 2},
                    separators=(",", ":"),
                ),
            )
            _assert_dscp_result(stopped, 0, 0, f"{context}; STOP cleanup")


@pytest.mark.feature(name="dscp")
@pytest.mark.profile(
    section="platform_hal_resetDscpCounts",
    groups=("valid", "invalid"),
    inputs={"interface_type": "interfaceType"},
    expected={"expected_status": "expected_status"},
)
def test_65_platform_reset_dscp_counts(
    raft_env, api_name, case_type, interface_type, expected_status,
):
    """Check JSON reset on a dedicated DUT/mock; counters cannot be restored."""
    context = f"{case_type}: interfaceType={interface_type!r}"
    result = run(
        "platform", raft_env, api_name,
        parameters_json=json.dumps(
            {"interfaceType": interface_type}, separators=(",", ":"),
        ),
    )
    _assert_dscp_result(
        result, expected_status, 0 if case_type == "valid" else 1, context,
    )


@pytest.mark.feature(name="dscp")
@pytest.mark.profile(
    section="platform_hal_getDscpClientList",
    groups=("valid", "invalid"),
    inputs={
        "interface_type": "interfaceType",
        "output_pointer": "output_pointer",
    },
    expected={"expected_status": "expected_status"},
)
def test_66_platform_get_dscp_client_list(
    raft_env, api_name, case_type, interface_type, output_pointer, expected_status,
):
    """Validate the default page and actual HAL NULL-pointer rejection."""
    context = (
        f"{case_type}: interfaceType={interface_type!r}, "
        f"output_pointer={output_pointer!r}"
    )
    result = run(
        "platform", raft_env, api_name,
        parameters={"interfaceType": interface_type},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    if case_type == "valid":
        expected_exit = 0
    elif interface_type not in (1, 2, 3, 4):
        expected_exit = 1
    else:
        assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        expected_exit = 1
    _assert_dscp_result(result, expected_status, expected_exit, context)
    response = result["output"]
    if case_type != "valid":
        for prefix in ("numElements:", "elementOffset:", "elementCount:", "element["):
            assert_equal(
                any(line.startswith(prefix) for line in response.splitlines()),
                False, f"No {prefix} data on failure", context,
            )
        return
    num_elements = field(response, "numElements")
    element_offset = field(response, "elementOffset")
    element_count = field(response, "elementCount")
    assert_equal(
        0 <= num_elements <= 63, True, "Element count within header range", context,
    )
    assert_equal(element_offset, 0, "Default element offset", context)
    assert_equal(
        element_count, min(num_elements, 1), "Default element page size", context,
    )
    for index in range(element_offset, element_offset + element_count):
        prefix = f"element[{index}]"
        dscp_value = field(response, f"{prefix}.dscp_value")
        num_clients = field(response, f"{prefix}.numClients")
        client_offset = field(response, f"{prefix}.clientOffset")
        client_count = field(response, f"{prefix}.clientCount")
        assert_equal(
            0 <= dscp_value <= 65534, True, "DSCP value range", context,
        )
        assert_equal(
            0 <= num_clients <= 255, True, "Client count within header range", context,
        )
        assert_equal(client_offset, 0, "Default client offset", context)
        assert_equal(
            client_count, min(num_clients, 16), "Default client page size", context,
        )
        for client_index in range(client_count):
            client_prefix = f"{prefix}.client[{client_index}]"
            mac = string_field(response, f"{client_prefix}.mac")
            octets = mac.split(":")
            valid_mac = len(octets) == 6 and all(
                len(octet) == 2
                and all(char in "0123456789abcdefABCDEF" for char in octet)
                for octet in octets
            )
            assert_equal(valid_mac, True, "Client MAC format", context)
            for counter in ("rxBytes", "txBytes"):
                value = field(response, f"{client_prefix}.{counter}")
                assert_equal(
                    0 <= value <= (1 << 64) - 1, True,
                    f"{counter} range", context,
                )


@pytest.mark.feature(name="firmware_bank")
@pytest.mark.profile(
    section="platform_hal_GetFirmwareBankInfo",
    groups=("valid", "invalid"),
    inputs={
        "bank_index": "bankIndex",
        "output_pointer": "output_pointer",
    },
    expected={"expected_status": "expected_status"},
)
def test_67_platform_get_firmware_bank_info(
    raft_env, api_name, case_type, bank_index, output_pointer, expected_status,
):
    """Validate firmware fields, enum validation, and actual HAL NULL rejection."""
    result = run(
        "platform", raft_env, api_name,
        parameters={"bankIndex": bank_index},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = (
        f"{case_type}: bankIndex={bank_index!r}, "
        f"output_pointer={output_pointer!r}"
    )
    assert_equal(
        field(response, "status"), expected_status,
        "Firmware-bank API status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        for name in ("fw_name", "fw_state"):
            assert_equal(
                bool(string_field(response, name).strip()), True,
                f"{name} is nonempty", context,
            )
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful GET", context,
        )
    else:
        # Invalid banks and NULL output pointers are forwarded to HAL.
        if bank_index in (0, 1):
            assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
        expected_returncode = 1
        expected_error = "HAL operation failed"
        assert_equal(
            result["returncode"], expected_returncode, "Failure CLI exit", context,
        )
        assert_equal(
            string_field(response, "error"), expected_error,
            "Failure error", context,
        )
        for name in ("fw_name", "fw_state"):
            assert_equal(
                any(line.startswith(f"{name}:") for line in response.splitlines()),
                False, f"No {name} output on failure", context,
            )


@pytest.mark.feature(name="web_access")
@pytest.mark.profile(
    section="platform_hal_SetWebAccessLevel",
    groups=("valid", "invalid"),
    inputs={"user_index": "userIndex", "if_index": "ifIndex", "value": "value"},
    expected={"expected_status": "expected_status"},
)
def test_68_platform_set_web_access_level(
    raft_env, api_name, case_type, user_index, if_index, value, expected_status,
):
    """JSON access-policy SET cases without snapshot, readback, or restoration.

    Do not run boundary-policy changes on unapproved real devices.
    Successful SETs leave the requested access levels in place.
    """
    parameters = {"userIndex": user_index, "ifIndex": if_index}
    context = (
        f"{case_type}: userIndex={user_index!r}, ifIndex={if_index!r}, "
        f"value={value!r}"
    )
    result = run(
        "platform", raft_env, api_name,
        parameters_json=json.dumps(
            {**parameters, "value": value}, separators=(",", ":"),
        ),
    )
    response = result["output"]
    assert_equal(
        field(response, "status"), expected_status, "SET API status", context,
    )
    assert_equal(
        result["returncode"], 0 if case_type == "valid" else 1,
        "SET CLI exit", context,
    )
    if case_type == "valid":
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful SET", context,
        )
    else:
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "HAL rejection error", context,
        )


@pytest.mark.feature(name="interface_stats")
@pytest.mark.profile(
    section="platform_hal_GetInterfaceStats",
    groups=("valid", "invalid"),
    inputs={"ifname": "ifname", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_69_platform_get_interface_stats(
    raft_env, api_name, case_type, ifname, output_pointer, expected_status,
):
    """Validate counters and HAL failures using profile-selected interfaces."""
    # Explicit CLI token is converted to an actual C NULL by the adapter.
    parameters = {"ifname": "NULL" if ifname is None else ifname}
    result = run(
        "platform", raft_env, api_name, parameters=parameters,
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = (
        f"{case_type}: ifname={ifname!r}, output_pointer={output_pointer!r}"
    )
    counter_names = ("rx_packet", "tx_packet", "rx_bytes", "tx_bytes")
    assert_equal(
        field(response, "status"), expected_status, "Interface stats status", context,
    )
    # Every invalid case in this profile is a HAL rejection, not a CLI error.
    assert_equal(
        result["returncode"], 0 if case_type == "valid" else 1,
        "Interface stats CLI exit", context,
    )
    if case_type == "valid":
        for name in counter_names:
            actual = field(response, name)
            assert_equal(
                0 <= actual <= (1 << 64) - 1, True,
                f"{name} is an unsigned 64-bit counter", context,
            )
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful GET", context,
        )
    else:
        assert_equal(
            string_field(response, "error"), "HAL operation failed",
            "HAL failure response", context,
        )
        for name in counter_names:
            assert_equal(
                any(line.startswith(f"{name}:") for line in response.splitlines()),
                False, f"No {name} returned after failure", context,
            )


@pytest.mark.feature(name="qos")
@pytest.mark.profile(
    section="platform_hal_qos_apply",
    groups=("valid", "invalid"),
    inputs={"params": "params"},
    expected={"expected_status": "expected_status"},
)
def test_70_platform_qos_apply(
    raft_env, api_name, case_type, params, expected_status,
):
    """Mock/dedicated DUT only: applying QoS changes state with no restore API.

    A profile object supplies flat API JSON; profile null uses --params=NULL.
    Assertions verify reported status, not traffic marking or shaping.
    """
    if params is None:
        result = run(
            "platform", raft_env, api_name, parameters={"params": "NULL"},
        )
    else:
        assert isinstance(params, dict), "params must be an object or null"
        assert set(params) == {
            "src_mac", "ip_version", "src_ip", "dest_ip",
            "src_port", "dest_port", "protocol", "dscp_value",
        }, "params must contain exactly the eight QoS fields"
        result = run(
            "platform", raft_env, api_name,
            parameters_json=json.dumps(params, separators=(",", ":")),
        )

    actual_status = field(result["output"], "status")
    assert_equal(
        actual_status, expected_status, "QoS status",
        (
            f"{case_type}: params={params!r}; "
            f"expected_status={expected_status}; "
            f"actual_status={actual_status}; result={result}"
        ),
    )


@pytest.mark.feature(name="web_access")
@pytest.mark.profile(
    section="platform_hal_GetWebAccessLevel",
    groups=("valid", "invalid"),
    inputs={
        "user_index": "userIndex",
        "if_index": "ifIndex",
        "output_pointer": "output_pointer",
    },
    expected={"expected_status": "expected_status"},
)
def test_71_platform_get_web_access_level(
    raft_env, api_name, case_type, user_index, if_index,
    output_pointer, expected_status,
):
    """Validate GET ranges, adapter rejection, and actual HAL NULL rejection."""
    result = run(
        "platform", raft_env, api_name,
        parameters={"userIndex": user_index, "ifIndex": if_index},
        output_pointer=None if output_pointer == "DEFAULT" else output_pointer,
    )
    response = result["output"]
    context = (
        f"{case_type}: userIndex={user_index!r}, ifIndex={if_index!r}, "
        f"output_pointer={output_pointer!r}"
    )
    assert_equal(
        field(response, "status"), expected_status, "GET API status", context,
    )
    if case_type == "valid":
        assert_equal(result["returncode"], 0, "Successful CLI exit", context)
        actual_value = field(response, "value")
        assert_equal(
            0 <= actual_value <= 2147483647, True,
            "Web access level is within the header range", context,
        )
        assert_equal(
            any(line.startswith("error:") for line in response.splitlines()),
            False, "No error on successful GET", context,
        )
    else:
        invalid_index = (
            not -2147483648 <= user_index <= 2147483647
            or not -2147483648 <= if_index <= 2147483647
        )
        if invalid_index:
            expected_returncode = 2
            expected_error = "invalid argument value"
        else:
            assert_equal(output_pointer, "NULL", "HAL NULL-pointer case", context)
            expected_returncode = 1
            expected_error = "HAL operation failed"
        assert_equal(
            result["returncode"], expected_returncode, "Failure CLI exit", context,
        )
        assert_equal(
            string_field(response, "error"), expected_error,
            "Failure error", context,
        )
        assert_equal(
            any(line.startswith("value:") for line in response.splitlines()),
            False, "No value on failed GET", context,
        )
