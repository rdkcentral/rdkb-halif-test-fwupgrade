"""EthSW L1: static HAL contracts (python) and profile-driven dynamic cases (JSON).

Static rows of ETHSW_HAL_Testcases.xlsx (sheet L1_Testcases, column "static") are
declared here with hal_case metadata. Rows marked "dynamic" (SetAgingSpeed,
LocatePortByMacAddress, ethPortConfigure, getCurrentWanHWConf) are loaded from
configs/ethsw_hal.json through profile markers.

Return codes: RETURN_OK = 0, RETURN_ERR = -1. The CLI exits 0 for a valid HAL result
and 1 when the HAL returns an error. NULL pointers are exercised with the
--output-pointer token (DEFAULT = valid buffer, NULL_<NAME> = that pointer is NULL).
State-changing tests restore the mock DUT state they touch.

Port numbering (CCSP_HAL_ETHSW_PORT): EthPort1-8 = 1-8, Moca1-2 = 9-10,
Wlan1-4 = 11-14, Processor1-2 = 15-16, InterconnectPort1-2 = 17-18,
MgmtPort = 19, PortMax = 20 (MgmtPort/PortMax are rejected by the HAL).
"""
from contextlib import contextmanager
import json

import pytest

from framework.execution import assert_equal, field, restore_fields, run

pytestmark = [pytest.mark.raft, pytest.mark.component("ethsw"), pytest.mark.L1]

COMPONENT = "ethsw"

# --- Port groups -----------------------------------------------------------
ETH_PORTS = tuple(range(1, 9))            # CCSP_HAL_ETHSW_EthPort1-8
MOCA_PORTS = (9, 10)                      # CCSP_HAL_ETHSW_Moca1-2
WLAN_PORTS = tuple(range(11, 15))         # CCSP_HAL_ETHSW_Wlan1-4
PROCESSOR_PORTS = (15, 16)                # CCSP_HAL_ETHSW_Processor1-2
INTERCONNECT_PORTS = (17, 18)             # CCSP_HAL_ETHSW_InterconnectPort1-2
OTHER_VALID_PORTS = MOCA_PORTS + WLAN_PORTS + PROCESSOR_PORTS + INTERCONNECT_PORTS
VALID_PORTS = ETH_PORTS + OTHER_VALID_PORTS
MGMT_PORT = 19                            # CCSP_HAL_ETHSW_MgmtPort (rejected)
PORT_MAX = 20                             # CCSP_HAL_ETHSW_PortMax (rejected)
INVALID_PORT = -1                         # "invalid portid"
INVALID_PORTS = (MGMT_PORT, PORT_MAX, INVALID_PORT)

# --- Enumerations ----------------------------------------------------------
LINK_RATES = tuple(range(0, 8))           # LINK_NULL, 10M, 100M, 1G, 2.5G, 5G, 10G, Auto
LINK_1GBPS, LINK_AUTO = 3, 7
DUPLEX_MODES = (0, 1, 2)                  # Auto, Half, Full
DUPLEX_AUTO, DUPLEX_FULL = 0, 2
ADMIN_STATUSES = (0, 1, 2)                # AdminUp, AdminDown, AdminTest
ADMIN_UP = 0
INVALID_LINK_RATES = (-1, 8, 99)
INVALID_DUPLEX_MODES = (-1, 3, 99)
INVALID_ADMIN_STATUSES = (-1, 3, 99)


def _cases(api, rows):
    """Turn (case_type, *inputs) rows into pytest params with hal_case metadata.

    Case IDs are numbered per API and category, in row order. Exact duplicate
    rows (the sheet lists overlapping subsets) are executed only once.
    """
    params, seen, counters = [], set(), {}
    for row in rows:
        if row in seen:
            continue
        seen.add(row)
        case_type = row[0]
        counters[case_type] = counters.get(case_type, 0) + 1
        params.append(pytest.param(*row, marks=pytest.mark.hal_case(
            api=api, case_type=case_type, case_id=counters[case_type],
        )))
    return params


def _pointer(output_pointer):
    return None if output_pointer == "DEFAULT" else output_pointer


def _status(result):
    return field(result["output"], "status")


def _check(result, case_type, expected_status, context):
    """Assert API status and the matching CLI exit code (0 OK, 1 HAL error)."""
    detail = f"{case_type}: {context}; result={result}"
    assert_equal(_status(result), expected_status, "API status", detail)
    assert_equal(result["returncode"], 0 if expected_status == 0 else 1, "CLI exit", detail)


@contextmanager
def _restore_port_state(raft_env, get_api, set_api, port_id, fields):
    """Save per-port state before a SET and restore it afterwards.

    fields maps SET parameter names to GET response fields. Ports the HAL
    rejects have no state to save, so nothing is restored for them.
    """
    snapshot = run(COMPONENT, raft_env, get_api, parameters={"PortId": port_id})
    saved = None
    if snapshot["returncode"] == 0 and _status(snapshot) == 0:
        saved = {name: field(snapshot["output"], source) for name, source in fields.items()}
    try:
        yield
    finally:
        if saved is not None:
            restored = run(COMPONENT, raft_env, set_api,
                           parameters={"PortId": port_id, **saved})
            assert_equal(_status(restored), 0, "restore status",
                         f"{set_api}: port={port_id}; saved={saved}")


@contextmanager
def _restore_wan_state(raft_env):
    """Restore EthWAN enable flag and port, which several tests change."""
    with restore_fields(
        COMPONENT, raft_env,
        get_api="CcspHalExtSw_getEthWanEnable", set_api="CcspHalExtSw_setEthWanEnable",
        fields={"Flag": "enable"},
    ):
        with restore_fields(
            COMPONENT, raft_env,
            get_api="CcspHalExtSw_getEthWanPort", set_api="CcspHalExtSw_setEthWanPort",
            fields={"Port": "port"},
        ):
            yield



@pytest.mark.feature(name='init')
@pytest.mark.parametrize(
    "case_type, expected_status",
    _cases("CcspHalEthSwInit", [("valid", 0)]),
)
def test_01_ethsw_init(raft_env, case_type, expected_status):
    result = run(COMPONENT, raft_env, "CcspHalEthSwInit")
    _check(result, case_type, expected_status, "initial call")



@pytest.mark.feature(name='port_status')
@pytest.mark.parametrize(
    "case_type, port_id, output_pointer, expected_status",
    _cases("CcspHalEthSwGetPortStatus", [
        *[("valid", port, "DEFAULT", 0) for port in VALID_PORTS],                 # rows 2-6
        *[("invalid", port, "DEFAULT", -1) for port in INVALID_PORTS],            # rows 7-9
        ("invalid", 1, "NULL_LINK_RATE", -1),                                     # row 10
        ("invalid", 1, "NULL_DUPLEX_MODE", -1),                                   # row 11
        ("invalid", 1, "NULL_STATUS", -1),                                        # row 12
    ]),
)
def test_02_ethsw_get_port_status(raft_env, case_type, port_id, output_pointer, expected_status):
    result = run(
        COMPONENT, raft_env, "CcspHalEthSwGetPortStatus",
        parameters={"PortId": port_id}, output_pointer=_pointer(output_pointer),
    )
    _check(result, case_type, expected_status, f"PortId={port_id}, output_pointer={output_pointer}")
    if case_type == "valid":
        for name in ("link_rate", "duplex_mode", "link_status"):
            field(result["output"], name)



@pytest.mark.feature(name='port_cfg')
@pytest.mark.parametrize(
    "case_type, port_id, output_pointer, expected_status",
    _cases("CcspHalEthSwGetPortCfg", [
        *[("valid", port, "DEFAULT", 0) for port in VALID_PORTS],                 # rows 13-17
        *[("invalid", port, "DEFAULT", -1) for port in INVALID_PORTS],            # rows 18-20
        ("invalid", 1, "NULL_LINK_RATE", -1),                                     # row 21
        ("invalid", 1, "NULL_DUPLEX_MODE", -1),                                   # row 22
    ]),
)
def test_03_ethsw_get_port_cfg(raft_env, case_type, port_id, output_pointer, expected_status):
    result = run(
        COMPONENT, raft_env, "CcspHalEthSwGetPortCfg",
        parameters={"PortId": port_id}, output_pointer=_pointer(output_pointer),
    )
    _check(result, case_type, expected_status, f"PortId={port_id}, output_pointer={output_pointer}")
    if case_type == "valid":
        for name in ("link_rate", "duplex_mode"):
            field(result["output"], name)



@pytest.mark.feature(name='port_cfg')
@pytest.mark.parametrize(
    "case_type, port_id, link_rate, duplex_mode, expected_status",
    _cases("CcspHalEthSwSetPortCfg", [
        # Row 23: EthPort1-8 x every LinkRate x every DuplexMode.
        *[("valid", port, rate, duplex, 0)
          for port in ETH_PORTS for rate in LINK_RATES for duplex in DUPLEX_MODES],
        # Rows 24-27: other valid port types with LINK_Auto / DUPLEX_Auto.
        *[("valid", port, LINK_AUTO, DUPLEX_AUTO, 0) for port in OTHER_VALID_PORTS],
        # Rows 28-30: invalid PortId.
        *[("invalid", port, LINK_AUTO, DUPLEX_AUTO, -1) for port in INVALID_PORTS],
        # Row 31: EthPort1, every LinkRate, DUPLEX_Full.
        *[("valid", 1, rate, DUPLEX_FULL, 0) for rate in LINK_RATES],
        # Row 32: EthPort1, invalid LinkRate.
        *[("invalid", 1, rate, DUPLEX_FULL, -1) for rate in INVALID_LINK_RATES],
        # Row 33: EthPort1, LINK_1Gbps, every DuplexMode.
        *[("valid", 1, LINK_1GBPS, duplex, 0) for duplex in DUPLEX_MODES],
        # Row 34: EthPort1, LINK_1Gbps, invalid DuplexMode.
        *[("invalid", 1, LINK_1GBPS, duplex, -1) for duplex in INVALID_DUPLEX_MODES],
    ]),
)
def test_04_ethsw_set_port_cfg(
    raft_env, case_type, port_id, link_rate, duplex_mode, expected_status,
):
    with _restore_port_state(
        raft_env, "CcspHalEthSwGetPortCfg", "CcspHalEthSwSetPortCfg", port_id,
        {"LinkRate": "link_rate", "DuplexMode": "duplex_mode"},
    ):
        result = run(
            COMPONENT, raft_env, "CcspHalEthSwSetPortCfg",
            parameters={"PortId": port_id, "LinkRate": link_rate, "DuplexMode": duplex_mode},
        )
        _check(result, case_type, expected_status,
               f"PortId={port_id}, LinkRate={link_rate}, DuplexMode={duplex_mode}")



@pytest.mark.feature(name='port_admin_status')
@pytest.mark.parametrize(
    "case_type, port_id, admin_status, expected_status",
    _cases("CcspHalEthSwSetPortAdminStatus", [
        # Rows 35-37: EthPort1-8 with AdminUp / AdminDown / AdminTest.
        *[("valid", port, admin, 0) for port in ETH_PORTS for admin in ADMIN_STATUSES],
        # Rows 38-41: other valid port types with AdminUp.
        *[("valid", port, ADMIN_UP, 0) for port in OTHER_VALID_PORTS],
        # Rows 42-44: invalid PortId.
        *[("invalid", port, ADMIN_UP, -1) for port in INVALID_PORTS],
        # Row 45: EthPort1 with invalid AdminStatus.
        *[("invalid", 1, admin, -1) for admin in INVALID_ADMIN_STATUSES],
    ]),
)
def test_05_ethsw_set_port_admin_status(
    raft_env, case_type, port_id, admin_status, expected_status,
):
    with _restore_port_state(
        raft_env, "CcspHalEthSwGetPortAdminStatus", "CcspHalEthSwSetPortAdminStatus", port_id,
        {"AdminStatus": "admin_status"},
    ):
        result = run(
            COMPONENT, raft_env, "CcspHalEthSwSetPortAdminStatus",
            parameters={"PortId": port_id, "AdminStatus": admin_status},
        )
        _check(result, case_type, expected_status,
               f"PortId={port_id}, AdminStatus={admin_status}")



@pytest.mark.feature(name='port_admin_status')
@pytest.mark.parametrize(
    "case_type, port_id, output_pointer, expected_status",
    _cases("CcspHalEthSwGetPortAdminStatus", [
        *[("valid", port, "DEFAULT", 0) for port in VALID_PORTS],                 # rows 46-50
        *[("invalid", port, "DEFAULT", -1) for port in INVALID_PORTS],            # rows 51-53
        ("invalid", 1, "NULL_ADMIN_STATUS", -1),                                  # row 54
    ]),
)
def test_06_ethsw_get_port_admin_status(
    raft_env, case_type, port_id, output_pointer, expected_status,
):
    result = run(
        COMPONENT, raft_env, "CcspHalEthSwGetPortAdminStatus",
        parameters={"PortId": port_id}, output_pointer=_pointer(output_pointer),
    )
    _check(result, case_type, expected_status, f"PortId={port_id}, output_pointer={output_pointer}")
    if case_type == "valid":
        field(result["output"], "admin_status")



@pytest.mark.feature(name='aging_speed')
@pytest.mark.profile(
    section="CcspHalEthSwSetAgingSpeed",
    groups=("valid", "invalid"),
    inputs={"port_id": "PortId", "aging_speed": "AgingSpeed"},
    expected={"expected_status": "expected_status"},
)
def test_07_ethsw_set_aging_speed(
    raft_env, api_name, case_type, port_id, aging_speed, expected_status,
):
    """Profile-driven aging-speed SET; the aging time is not read back by the HAL."""
    result = run(
        COMPONENT, raft_env, api_name,
        parameters={"PortId": port_id, "AgingSpeed": aging_speed},
    )
    _check(result, case_type, expected_status,
           f"PortId={port_id}, AgingSpeed={aging_speed}")



@pytest.mark.feature(name='locate_port')
@pytest.mark.profile(
    section="CcspHalEthSwLocatePortByMacAddress",
    groups=("valid", "invalid"),
    inputs={"mac": "mac", "output_pointer": "output_pointer"},
    expected={"expected_status": "expected_status"},
)
def test_08_ethsw_locate_port_by_mac(
    raft_env, api_name, case_type, mac, output_pointer, expected_status,
):
    if mac == "NULL":
        result = run(COMPONENT, raft_env, api_name,
                     parameters_json=json.dumps({"mac": None}, separators=(",", ":")))
    else:
        result = run(COMPONENT, raft_env, api_name, parameters={"mac": mac},
                     output_pointer=_pointer(output_pointer))
    _check(result, case_type, expected_status, f"mac={mac!r}, output_pointer={output_pointer}")
    if case_type == "valid":
        assert field(result["output"], "port") >= 1, f"Located port must be 1-based: {result}"



@pytest.mark.feature(name='associated_device')
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    _cases("CcspHalExtSw_getAssociatedDevice", [
        ("valid", "DEFAULT", 0),                                                  # row 67
        ("invalid", "NULL_SIZE", -1),                                             # row 68
        ("invalid", "NULL_DEVICES", -1),                                          # row 69
    ]),
)
def test_09_ethsw_get_associated_device(raft_env, case_type, output_pointer, expected_status):
    result = run(COMPONENT, raft_env, "CcspHalExtSw_getAssociatedDevice",
                 output_pointer=_pointer(output_pointer))
    _check(result, case_type, expected_status, f"output_pointer={output_pointer}")
    if case_type == "valid":
        assert field(result["output"], "count") >= 0



@pytest.mark.feature(name='eth_port_configure')
@pytest.mark.profile(
    section="CcspHalExtSw_ethPortConfigure",
    groups=("valid", "invalid"),
    inputs={"ifname": "ifname", "wan_mode": "WanMode"},
    expected={"expected_status": "expected_status"},
)
def test_10_ethsw_eth_port_configure(
    raft_env, api_name, case_type, ifname, wan_mode, expected_status,
):
    payload = json.dumps({"ifname": ifname, "WanMode": wan_mode}, separators=(",", ":"))
    with _restore_wan_state(raft_env):
        result = run(COMPONENT, raft_env, api_name, parameters_json=payload)
        _check(result, case_type, expected_status, f"ifname={ifname!r}, WanMode={wan_mode}")


@pytest.mark.feature(name='eth_wan_enable')
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    _cases("CcspHalExtSw_getEthWanEnable", [
        ("valid", "DEFAULT", 0),                                                  # row 74
        ("invalid", "NULL_FLAG", -1),                                             # row 75
    ]),
)
def test_11_ethsw_get_eth_wan_enable(raft_env, case_type, output_pointer, expected_status):
    result = run(COMPONENT, raft_env, "CcspHalExtSw_getEthWanEnable",
                 output_pointer=_pointer(output_pointer))
    _check(result, case_type, expected_status, f"output_pointer={output_pointer}")
    if case_type == "valid":
        assert field(result["output"], "enable") in (0, 1)



@pytest.mark.feature(name='eth_wan_enable')
@pytest.mark.parametrize(
    "case_type, flag, expected_status",
    _cases("CcspHalExtSw_setEthWanEnable", [
        ("valid", 1, 0),                                                          # row 76 (TRUE)
        ("valid", 0, 0),                                                          # row 77 (FALSE)
    ]),
)
def test_12_ethsw_set_eth_wan_enable(raft_env, case_type, flag, expected_status):
    with restore_fields(
        COMPONENT, raft_env,
        get_api="CcspHalExtSw_getEthWanEnable", set_api="CcspHalExtSw_setEthWanEnable",
        fields={"Flag": "enable"},
    ):
        result = run(COMPONENT, raft_env, "CcspHalExtSw_setEthWanEnable",
                     parameters={"Flag": flag})
        _check(result, case_type, expected_status, f"Flag={flag}")



@pytest.mark.feature(name='wan_hw_conf')
@pytest.mark.profile(
    section="CcspHalExtSw_getCurrentWanHWConf",
    groups=("valid",),
    inputs={"expected_result": "ExpectedResult"},
)
def test_13_ethsw_get_current_wan_hw_conf(
    raft_env, api_name, case_type, expected_result,
):
    result = run(COMPONENT, raft_env, api_name)
    _check(result, case_type, 0, "read WAN hardware configuration")
    assert_equal(field(result["output"], "wan_hw_conf"), expected_result,
                 "WAN HW configuration", f"{case_type}: result={result}")



@pytest.mark.feature(name='eth_wan_port')
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    _cases("CcspHalExtSw_getEthWanPort", [
        ("valid", "DEFAULT", 0),                                                  # row 79
        ("invalid", "NULL_PORT", -1),                                             # row 80
    ]),
)
def test_14_ethsw_get_eth_wan_port(raft_env, case_type, output_pointer, expected_status):
    result = run(COMPONENT, raft_env, "CcspHalExtSw_getEthWanPort",
                 output_pointer=_pointer(output_pointer))
    _check(result, case_type, expected_status, f"output_pointer={output_pointer}")
    if case_type == "valid":
        assert 0 <= field(result["output"], "port") <= 19


@pytest.mark.feature(name='eth_wan_port')
@pytest.mark.parametrize(
    "case_type, port, expected_status",
    _cases("CcspHalExtSw_setEthWanPort", [
        # Rows 81-82: every valid zero-based WAN port 0..19 (not the PORT enum).
        *[("valid", port, 0) for port in range(20)],
        ("invalid", 20, -1),                                                      # row 83
    ]),
)
def test_15_ethsw_set_eth_wan_port(raft_env, case_type, port, expected_status):
    with restore_fields(
        COMPONENT, raft_env,
        get_api="CcspHalExtSw_getEthWanPort", set_api="CcspHalExtSw_setEthWanPort",
        fields={"Port": "port"},
    ):
        result = run(COMPONENT, raft_env, "CcspHalExtSw_setEthWanPort", parameters={"Port": port})
        _check(result, case_type, expected_status, f"Port={port}")



@pytest.mark.feature(name='port_stats')
@pytest.mark.parametrize(
    "case_type, port_id, output_pointer, expected_status",
    _cases("CcspHalEthSwGetEthPortStats", [
        *[("valid", port, "DEFAULT", 0) for port in VALID_PORTS],                 # rows 84-101
        ("invalid", MGMT_PORT, "DEFAULT", -1),                                    # row 102
        ("invalid", PORT_MAX, "DEFAULT", -1),                                     # row 103
        ("invalid", 1, "NULL_STATS", -1),                                         # row 104
        ("invalid", INVALID_PORT, "NULL_STATS", -1),                              # row 105
    ]),
)
def test_16_ethsw_get_eth_port_stats(raft_env, case_type, port_id, output_pointer, expected_status):
    result = run(
        COMPONENT, raft_env, "CcspHalEthSwGetEthPortStats",
        parameters={"PortId": port_id}, output_pointer=_pointer(output_pointer),
    )
    _check(result, case_type, expected_status, f"PortId={port_id}, output_pointer={output_pointer}")
    if case_type == "valid":
        for name in ("BytesSent", "BytesReceived", "PacketsSent", "PacketsReceived"):
            field(result["output"], name)



@pytest.mark.feature(name='eth_wan_link')
@pytest.mark.parametrize(
    "case_type, expected_link_status",
    _cases("GWP_GetEthWanLinkStatus", [("valid", 1)]),                            # row 106
)
def test_17_ethsw_get_eth_wan_link_status(raft_env, case_type, expected_link_status):
    with _restore_wan_state(raft_env):
        for api, parameters in (("CcspHalExtSw_setEthWanEnable", {"Flag": 1}),
                                ("CcspHalExtSw_setEthWanPort", {"Port": 0})):
            prepared = run(COMPONENT, raft_env, api, parameters=parameters)
            _check(prepared, case_type, 0, f"precondition {api} {parameters}")
        result = run(COMPONENT, raft_env, "GWP_GetEthWanLinkStatus")
        _check(result, case_type, 0, "read EthWAN link status")
        assert_equal(field(result["output"], "link_status"), expected_link_status,
                     "EthWAN link status", f"{case_type}: result={result}")



@pytest.mark.feature(name='eth_wan_interface')
@pytest.mark.parametrize(
    "case_type, max_size, output_pointer, expected_status",
    _cases("GWP_GetEthWanInterfaceName", [
        ("valid", 262, "DEFAULT", 0),                                             # row 107
        ("valid", 11, "DEFAULT", 0),                                              # row 108
        ("invalid", 10, "DEFAULT", -1),                                           # row 109
        ("invalid", 263, "DEFAULT", -1),                                          # row 110
        ("invalid", 64, "NULL_BUFFER", -1),                                       # row 111
    ]),
)
def test_18_ethsw_get_eth_wan_interface_name(
    raft_env, case_type, max_size, output_pointer, expected_status,
):
    result = run(
        COMPONENT, raft_env, "GWP_GetEthWanInterfaceName",
        parameters={"maxSize": max_size}, output_pointer=_pointer(output_pointer),
    )
    _check(result, case_type, expected_status, f"maxSize={max_size}, output_pointer={output_pointer}")
    if case_type == "valid":
        assert "interface: " in result["output"], f"Missing interface name: {result}"


@pytest.mark.feature(name='eth_wan_callback')
@pytest.mark.parametrize(
    "case_type, callback, expected_status",
    _cases("GWP_RegisterEthWan_Callback", [
        ("valid", "ENABLE", 0),                                                   # row 112
        ("valid", "NULL", 0),                                                     # row 113
    ]),
)
def test_19_ethsw_register_eth_wan_callback(raft_env, case_type, callback, expected_status):
    result = run(COMPONENT, raft_env, "GWP_RegisterEthWan_Callback",
                 parameters={"callback": callback})
    _check(result, case_type, expected_status, f"callback={callback}")
    assert f"callback: {callback}" in result["output"], f"Unexpected response: {result}"


@pytest.mark.feature(name='associated_device_callback')
@pytest.mark.parametrize(
    "case_type, callback, expected_status",
    _cases("CcspHalExtSw_ethAssociatedDevice_callback_register", [
        ("valid", "ENABLE", 0),                                                   # row 114
        ("valid", "NULL", 0),                                                     # row 115
    ]),
)
def test_20_ethsw_register_associated_device_callback(
    raft_env, case_type, callback, expected_status,
):
    result = run(COMPONENT, raft_env, "CcspHalExtSw_ethAssociatedDevice_callback_register",
                 parameters={"callback": callback})
    _check(result, case_type, expected_status, f"callback={callback}")
    assert f"callback: {callback}" in result["output"], f"Unexpected response: {result}"
