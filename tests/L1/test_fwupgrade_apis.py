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
"""FWUpgrade L1: static in-script cases and profile-driven dynamic cases.

Static cases (Excel rows 11-21: download interface, download, status, reboot,
factory reset) keep their inputs in this script as ``pytest.param`` rows with
``hal_case`` metadata. Dynamic cases (Excel rows 1-10 and 22-29: set/get download
URL and download_install) take their inputs from configs/fwupgrade_hal.json.

The script is library-agnostic: it only uses the public fwupgrade_hal.h API and
never assumes the defaults of the bundled stand-in HAL. Everything that depends
on the library under test (a reachable download URL, filename and interface)
comes from the ``download_fixture`` section of the profile, so a different
libfwupgrade_hal.so is tested by editing the JSON only. Every state-changing test
runs inside ``restore_fields`` (as in the platform tests), which snapshots the GET
values and puts them back.
"""
import json
from contextlib import nullcontext

import pytest

from framework.core import selected_profile
from framework.execution import (
    assert_equal, field, restore_fields, run, run_reboot, text_field,
)
from framework.reporting import emit

pytestmark = [pytest.mark.raft, pytest.mark.component("fwupgrade"), pytest.mark.L1]

COMPONENT = "fwupgrade"


@pytest.fixture
def download_fixture(request):
    """Library-specific download settings from the selected profile."""
    fixture = selected_profile(request.config, COMPONENT).get("download_fixture")
    if (not isinstance(fixture, dict)
            or any(not isinstance(fixture.get(key), str) or not fixture[key]
                   for key in ("url", "filename"))
            or fixture.get("interface") not in (0, 1)):
        raise pytest.UsageError(
            "Profile requires download_fixture {url, filename, interface 0|1}")
    return fixture


@pytest.fixture
def reboot_recovery(request):
    """Optional 'reboot_recovery' profile section: settle/timeout/poll seconds."""
    recovery = selected_profile(request.config, COMPONENT).get("reboot_recovery", {})
    if not isinstance(recovery, dict):
        raise pytest.UsageError("Profile 'reboot_recovery' must be an object")
    return recovery


@pytest.fixture
def factory_reset_recovery(request):
    """Optional 'factory_reset_recovery' profile section: DUT reprovisioning hooks.

    A factory reset can wipe network/SSH settings or the dispatcher, which restoring
    fields cannot undo. host_command (test host) and dut_command (DUT) bring management
    access and the backend back; see execution.run_reboot. Defaults to no hooks.
    """
    section = selected_profile(request.config, COMPONENT).get("factory_reset_recovery", {})
    if not isinstance(section, dict):
        raise pytest.UsageError("Profile 'factory_reset_recovery' must be an object")
    return section


def _expand(value):
    """Expand '@repeat:<char>:<count>' profile tokens used for length-limit cases."""
    if isinstance(value, str) and value.startswith("@repeat:"):
        _, char, count = value.split(":", 2)
        assert len(char) == 1 and count.isdigit() and 0 < int(count) <= 4096, (
            f"Invalid repeat token: {value}")
        return char * int(count)
    return value


def _json(**fields):
    return json.dumps(fields, separators=(",", ":"), allow_nan=False)


# Documented fwupgrade_hal_get_download_status domain: 0 not started, 1-99
# progress, 100 complete, 400-407 and 500 failures.
_DOWNLOAD_STATUS_DOMAIN = {0, *range(1, 101), *range(400, 408), 500}
# Set once any download has been issued in this run; the HAL contract does not
# say configuration resets progress, so "not started" can only be assumed before.
_download_issued = False


def _hal(raft_env, api, payload=None, *, output_pointer=None, parameters=None):
    global _download_issued
    if api in ("download", "download_install"):
        _download_issued = True
    return run(COMPONENT, raft_env, f"fwupgrade_hal_{api}", payload,
               output_pointer=output_pointer, parameters=parameters)


def _hal_reboot(raft_env, api, recovery, payload=None, reprovision=None):
    """Call an API that reboots a real DUT and wait for the backend to come back.

    Cleanup (restore_fields) and later tests run only after this returns, so they
    never talk to a DUT that is still rebooting. See execution.run_reboot. Pass
    reprovision (factory_reset_recovery) for destructive resets.
    """
    return run_reboot(COMPONENT, raft_env, f"fwupgrade_hal_{api}", payload,
                      recovery=recovery, reprovision=reprovision)


def _precondition(raft_env, api, payload=None, **kwargs):
    result = _hal(raft_env, api, payload, **kwargs)
    assert_equal(field(result["output"], "status"), 0, f"precondition {api}",
                 f"precondition failed: {api}")
    return result


def _configure_download(raft_env, fixture):
    """Configure the download in the HAL spec order: URL, then interface."""
    _precondition(raft_env, "set_download_url",
                  _json(url=fixture["url"], filename=fixture["filename"]))
    _precondition(raft_env, "set_download_interface",
                  _json(interface=fixture["interface"]))


def _verify(result, case_type, expected_status, context):
    """Check the API status and the matching CLI exit (0 ok, 1 HAL rejection)."""
    actual_status = field(result["output"], "status")
    assert_equal(actual_status, expected_status, "API status", (
        f"{case_type}: {context}; expected status={expected_status}, "
        f"actual={actual_status}"))
    expected_exit = 0 if expected_status == 0 else 1
    assert_equal(result["returncode"], expected_exit, "CLI exit", (
        f"{case_type}: {context}; expected CLI exit={expected_exit}, "
        f"actual={result['returncode']}"))


def _verify_reboot(result, case_type, expected_status, context):
    """_verify() for a reboot-aware call.

    If the DUT rebooted before the framed response arrived, the HAL status cannot be
    read; run_reboot has already proven the reboot (boot id changed) and the recovery
    of the backend, so only the reboot itself is recorded.
    """
    if result["response_lost"]:
        emit("REBOOT", f"{case_type}: {context}; response lost to the reboot, HAL status "
             "not observable (DUT reboot and backend recovery verified)", "complete")
        assert_equal(expected_status, 0, "reboot only follows a successful request",
                     f"{case_type}: {context}; a lost response is only valid for status 0")
        return
    _verify(result, case_type, expected_status, context)


def _pointer(selector):
    assert selector in ("DEFAULT", "NULL"), f"Unsupported pointer selector: {selector}"
    return None if selector == "DEFAULT" else selector


# Restore blocks: same restore_fields mechanism as the platform tests. The
# interface is an integer; URL and filename are text (text_fields).
INTERFACE_FIELDS = dict(
    get_api="fwupgrade_hal_get_download_interface",
    set_api="fwupgrade_hal_set_download_interface",
    fields={"interface": "interface"},
    restore_as_json=True,
)
URL_FIELDS = dict(
    get_api="fwupgrade_hal_get_download_url",
    set_api="fwupgrade_hal_set_download_url",
    fields={},
    text_fields={"url": "url", "filename": "filename"},
    restore_as_json=True,
)


# ---------------------------------------------------------------------------
# Dynamic: download URL SET/GET, inputs from the selected JSON profile
# ---------------------------------------------------------------------------

@pytest.mark.feature(name="download_url")
@pytest.mark.profile(
    section="fwupgrade_hal_set_download_url",
    groups=("valid", "invalid"),
    inputs={"url": "url", "filename": "filename"},
    expected={"expected_status": "expected_status"},
)
def test_01_fwupgrade_set_download_url(
    raft_env, api_name, case_type, url, filename, expected_status,
):
    """Validate download URL SET using cases from the selected JSON profile."""
    with restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        url, filename = _expand(url), _expand(filename)
        result = run(COMPONENT, raft_env, api_name, _json(url=url, filename=filename))
        _verify(result, case_type, expected_status,
                f"url={url!r:.60}, filename={filename!r:.60}")
        if expected_status == 0:
            # Read back what was just configured; no assumption on defaults.
            stored = _precondition(raft_env, "get_download_url")["output"]
            assert_equal(text_field(stored, "url"), url, "stored URL")
            assert_equal(text_field(stored, "filename"), filename, "stored filename")


@pytest.mark.feature(name="download_url")
@pytest.mark.profile(
    section="fwupgrade_hal_get_download_url",
    groups=("valid", "invalid"),
    inputs={"url_pointer": "pUrl", "filename_pointer": "pFilename"},
    expected={"expected_status": "expected_status"},
)
def test_02_fwupgrade_get_download_url(
    raft_env, api_name, case_type, url_pointer, filename_pointer, expected_status,
):
    """Validate download URL GET using cases from the selected JSON profile."""
    # The CLI adapter nulls one output pointer per call: NULL = pUrl,
    # NULL_FILENAME = pFilename.
    nulls = []
    if _pointer(url_pointer):
        nulls.append("NULL")
    if _pointer(filename_pointer):
        nulls.append("NULL_FILENAME")
    assert len(nulls) <= 1, "The CLI can NULL only one output pointer per call"
    # GET is read-only: no snapshot, precondition SET or restore is needed. The
    # valid case only checks that the call succeeds and returns a URL and filename.
    result = run(COMPONENT, raft_env, api_name,
                 output_pointer=nulls[0] if nulls else None)
    _verify(result, case_type, expected_status,
            f"pUrl={url_pointer}, pFilename={filename_pointer}")
    if expected_status == 0:
        # text_field asserts exactly one such field is present in the response.
        text_field(result["output"], "url")
        text_field(result["output"], "filename")


# ---------------------------------------------------------------------------
# Static: inputs declared with each row (Excel rows 11-21)
# ---------------------------------------------------------------------------

@pytest.mark.feature(name="download_interface")
@pytest.mark.parametrize(
    "case_type, interface, expected_status",
    [
        pytest.param("valid", 0, 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_set_download_interface", case_type="valid", case_id=1,
        )),
        pytest.param("valid", 1, 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_set_download_interface", case_type="valid", case_id=2,
        )),
        pytest.param("invalid", 2, -1, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_set_download_interface", case_type="invalid", case_id=1,
        )),
    ],
)
def test_03_fwupgrade_set_download_interface(
    raft_env, case_type, interface, expected_status,
):
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS):
        result = _hal(raft_env, "set_download_interface", _json(interface=interface))
        _verify(result, case_type, expected_status, f"interface={interface}")


@pytest.mark.feature(name="download_interface")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_get_download_interface", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_get_download_interface", case_type="invalid", case_id=1,
        )),
    ],
)
def test_04_fwupgrade_get_download_interface(
    raft_env, case_type, output_pointer, expected_status,
):
    # GET is read-only: no snapshot, precondition SET or restore is needed. The
    # valid case only checks that the call succeeds and returns an interface value.
    result = _hal(raft_env, "get_download_interface",
                  output_pointer=_pointer(output_pointer))
    _verify(result, case_type, expected_status, f"output_pointer={output_pointer}")
    if expected_status == 0:
        # field() asserts exactly one integer interface field is in the response.
        field(result["output"], "interface")


@pytest.mark.feature(name="download")
@pytest.mark.parametrize(
    "case_type, expected_status",
    [
        pytest.param("valid", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_download", case_type="valid", case_id=1,
        )),
    ],
)
def test_05_fwupgrade_download(raft_env, download_fixture, case_type, expected_status):
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS), \
            restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        # Valid URL, filename and interface configured before the download.
        _configure_download(raft_env, download_fixture)
        result = _hal(raft_env, "download")
        _verify(result, case_type, expected_status, "download after configuration")


@pytest.mark.feature(name="download")
@pytest.mark.parametrize(
    "case_type, expected_status",
    [
        pytest.param("valid", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_get_download_status", case_type="valid", case_id=1,
        )),
    ],
)
def test_06_fwupgrade_get_download_status(
    raft_env, download_fixture, case_type, expected_status,
):
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS), \
            restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        _configure_download(raft_env, download_fixture)
        result = _hal(raft_env, "get_download_status")
        _verify(result, case_type, expected_status, "query before download start")
        status = field(result["output"], "download_status")
        if _download_issued:
            # An earlier download (e.g. test_05) may legitimately leave progress
            # retained; the HAL contract does not require configuration to reset it.
            assert status in _DOWNLOAD_STATUS_DOMAIN, (
                f"download status {status} is outside the documented domain")
        else:
            # Excel row 17: no download issued in this run, so expect 0.
            assert_equal(status, 0, "download status before download start")


@pytest.mark.feature(name="reboot")
@pytest.mark.parametrize(
    "case_type, output_pointer, expected_status",
    [
        pytest.param("valid", "DEFAULT", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_reboot_ready", case_type="valid", case_id=1,
        )),
        pytest.param("invalid", "NULL", -1, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_reboot_ready", case_type="invalid", case_id=1,
        )),
    ],
)
def test_07_fwupgrade_reboot_ready(
    raft_env, download_fixture, case_type, output_pointer, expected_status,
):
    if expected_status != 0:
        # NULL pValue is rejected before the upgrade state is looked at, so no
        # configuration or download is needed and nothing has to be restored.
        result = _hal(raft_env, "reboot_ready", output_pointer=_pointer(output_pointer))
        _verify(result, case_type, expected_status, f"output_pointer={output_pointer}")
        return
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS), \
            restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        # Reboot readiness is reported after a completed download/installation.
        # pValue 1 = ready for reboot after a successful firmware update
        # (2 = not ready for reboot, e.g. download in progress: L2 scope).
        _configure_download(raft_env, download_fixture)
        _precondition(raft_env, "download")
        result = _hal(raft_env, "reboot_ready", output_pointer=_pointer(output_pointer))
        _verify(result, case_type, expected_status, f"output_pointer={output_pointer}")
        assert_equal(field(result["output"], "reboot_ready"), 1,
                     "reboot_ready after successful download",
                     "pValue must be 1 (ready to reboot) after download and install")


@pytest.mark.feature(name="reboot")
@pytest.mark.parametrize(
    "case_type, expected_status",
    [
        pytest.param("valid", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_download_reboot_now", case_type="valid", case_id=1,
        )),
    ],
)
def test_08_fwupgrade_download_reboot_now(
    raft_env, download_fixture, reboot_recovery, case_type, expected_status,
):
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS), \
            restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        # Reboot request after a successful upgrade.
        _configure_download(raft_env, download_fixture)
        _precondition(raft_env, "download")
        ready = _precondition(raft_env, "reboot_ready")
        assert_equal(field(ready["output"], "reboot_ready"), 1,
                     "precondition reboot_ready", "system must be ready (1) to reboot")
        # A real HAL reboots the DUT here. Wait for the backend to return before
        # the restore_fields cleanup below (and any later test) uses it.
        result = _hal_reboot(raft_env, "download_reboot_now", reboot_recovery)
        _verify_reboot(result, case_type, expected_status, "reboot after successful upgrade")


@pytest.mark.feature(name="factory_reset")
@pytest.mark.parametrize(
    "case_type, expected_status",
    [
        pytest.param("valid", 0, marks=pytest.mark.hal_case(
            api="fwupgrade_hal_update_and_factoryreset", case_type="valid", case_id=1,
        )),
    ],
)
def test_09_fwupgrade_update_and_factoryreset(
    raft_env, download_fixture, reboot_recovery, factory_reset_recovery, case_type,
    expected_status,
):
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS), \
            restore_fields(COMPONENT, raft_env, **URL_FIELDS):
        # The API takes no input (no URL, filename, interface or image): it
        # updates from the download configured on the DUT. Configure the URL and
        # then the interface first (HAL spec order) so the test does not depend
        # on the library's defaults; both are restored afterwards.
        _configure_download(raft_env, download_fixture)
        # A successful factory reset can wipe network/SSH settings or the dispatcher.
        # Recover and reprovision the DUT (factory_reset_recovery hooks) before the
        # URL/interface restore above runs and before any later test starts.
        result = _hal_reboot(raft_env, "update_and_factoryreset", reboot_recovery,
                             reprovision=factory_reset_recovery)
        _verify_reboot(result, case_type, expected_status, "update and factory reset")


# ---------------------------------------------------------------------------
# Dynamic: direct install, inputs from the selected JSON profile
# ---------------------------------------------------------------------------

@pytest.mark.feature(name="download_install")
@pytest.mark.profile(
    section="fwupgrade_hal_download_install",
    groups=("valid", "invalid"),
    inputs={"url": "url"},
    expected={"expected_status": "expected_status"},
)
def test_10_fwupgrade_download_install(
    raft_env, download_fixture, reboot_recovery, api_name, case_type, url, expected_status,
):
    """Validate direct install using cases from the selected JSON profile.

    Every outcome (including an unreachable server) is driven by the URL in the
    profile; no mock failure injection is used, so a real library works as-is.
    """
    with restore_fields(COMPONENT, raft_env, **INTERFACE_FIELDS):
        # The URL is the API's only input, so set_download_url is not needed (and
        # the configured download URL is left untouched). The API has no
        # interface parameter, so set the interface first; it is restored after.
        _precondition(raft_env, "set_download_interface",
                      _json(interface=download_fixture["interface"]))
        url = _expand(url)
        if expected_status == 0:
            # A successful install may reboot a real DUT; rejected installs never do.
            result = run_reboot(COMPONENT, raft_env, api_name, _json(url=url),
                                recovery=reboot_recovery)
            _verify_reboot(result, case_type, expected_status, f"url={url!r:.60}")
        else:
            result = run(COMPONENT, raft_env, api_name, _json(url=url))
            _verify(result, case_type, expected_status, f"url={url!r:.60}")
