# halif-test-folder FWUpgrade HAL test execution

FWUpgrade HAL tests, laid out in the halif-test-folder structure:
`cli` and `dispatcher` (DUT-side HAL, server and CLI) and `framework`/`tests`/`configs`
(pytest/RAFT controller). The vendored RAFT runtime comes from TEST_MERGED, but
this tree has no runtime dependency on that checkout. By default the server is
linked with a bundled stateful stand-in HAL (`src/fwupgrade_hal.c`), which is
not a hardware driver; a vendor `libfwupgrade_hal.so` can be tested instead
(see "Testing a vendor libfwupgrade_hal.so").

## Layout

```
halif-test-folder/
  CMakeLists.txt, run_tests.py, pytest.ini, requirements-test.txt
  docs/                       this README
  cli/                        vl_cli.c, vl_generic.h (shared VL CLI, do not modify)
  dispatcher/src/             VL transport/server, fwupgrade HAL mock, dispatcher, component descriptor
  dispatcher/include/         vl_common.h, fwupgradehal_apis.h, fwupgrade_hal.h
  configs/                    rack.yml, rack.example.yml, device.yml, fwupgrade_hal.json
  framework/                  shared controller framework
  component/fwupgrade/        fwupgrade registration (settings.py)
  tests/L1/fwupgrade/         fwupgrade L1 tests (L2, L3 placeholders)
  third_party/                vendored RAFT runtime
  tools/
```

## Setup and build

Requirements: Linux, Python 3.10+, CMake, a C11 compiler. From the halif-test-folder root:

```bash
python3 -m venv .venv
.venv/bin/python -m pip install --no-input -r requirements-test.txt
cmake -S . -B build-run -DCMAKE_C_FLAGS="-Wall -Wextra -Werror"
cmake --build build-run --parallel 2
cmake --install build-run --prefix "$PWD/install-run"
```

All of these lines run from the halif-test-folder root (the `.venv` ends up at
`.venv`).

The CLI is the shared component-agnostic `vl_cli`; the controller passes the
socket, magic and API selector explicitly on every call.
Do not reuse relocated virtual environments or CMake caches.

## Testing a vendor libfwupgrade_hal.so

The DUT adapter (`dispatcher/src/fwupgradehal_apis.c`) calls only the
public API of `fwupgrade_hal.h`, exactly like the platform adapter, and the test
script uses only that adapter. So a different library needs no code change:

1. Build the server against the supplied library (it must export all ten
   `fwupgrade_hal_*` functions; CMake checks this with `nm -D`):

   ```bash
   cmake -S . -B build-run -DFWUPGRADE_HAL_LIB=/path/to/libfwupgrade_hal.so
   cmake --build build-run --parallel 2
   cmake --install build-run --prefix "$PWD/install-run"
   ```

   To return to the stand-in, reconfigure with `-DFWUPGRADE_HAL_LIB=`
   (empty; the value is cached). The vendor lib's header must match
   `dispatcher/include/fwupgrade_hal.h`.
2. Edit the library-specific values in `configs/fwupgrade_hal.json`:
   - `download_fixture`: a download `url`/`filename` and `interface` (0 or 1)
     that the library can really fetch. The tests assert `reboot_ready` is 1
     (ready to reboot) after a successful download and install; 2 (not ready)
     is covered at L2 (during an active download). `get_download_status` is
     asserted to be 0 after configuring the download, before it starts.
   - the `valid` cases of `fwupgrade_hal_set_download_url` and
     `fwupgrade_hal_download_install` (valid URL); the `invalid` cases hold
     malformed/unreachable URLs, including the unreachable-server case.
   No stand-in default or mock API is used by the tests.
3. Run the suite as described below.

Caution: against a real library `download`, `download_reboot_now`,
`update_and_factoryreset` and `download_install` really download, flash, reboot
or factory-reset the device. Use a lab device.

## Collection without a DUT

From the halif-test-folder root (after the venv setup above):

```bash
.venv/bin/python -B run_tests.py \
  --mode same-host --component=fwupgrade --level=L1 \
  -- --collect-only -q
```

The suite defines 29 fwupgrade L1 cases (11 valid, 18 invalid) following the
Static/Dynamic column of `FWUpgrade_HAL_L1_Testcases.xlsx`:

- **Static (11 cases)**: inputs live in `tests/L1/fwupgrade/test_fwupgrade_apis.py`
  as `pytest.param` rows carrying explicit `hal_case` metadata.
- **Dynamic (18 cases)**: inputs come from the standard JSON profile
  `configs/fwupgrade_hal.json` via `@pytest.mark.profile`; `hal_case` metadata is
  attached automatically from each profile `Case_ID`.

`Case_ID` is numbered independently within each API and valid/invalid category.
Excel row to (API, category, Case_ID):

| Excel rows | API | Kind | Category / Case_ID |
| --- | --- | --- | --- |
| 7 / 1-6 | `fwupgrade_hal_set_download_url` | Dynamic | valid 1 / invalid 1-6 |
| 10 / 8-9 | `fwupgrade_hal_get_download_url` | Dynamic | valid 1 / invalid 1-2 |
| 11-12 / 13 | `fwupgrade_hal_set_download_interface` | Static | valid 1-2 / invalid 1 |
| 15 / 14 | `fwupgrade_hal_get_download_interface` | Static | valid 1 / invalid 1 |
| 16 | `fwupgrade_hal_download` | Static | valid 1 |
| 17 | `fwupgrade_hal_get_download_status` | Static | valid 1 |
| 19 / 18 | `fwupgrade_hal_reboot_ready` | Static | valid 1 / invalid 1 |
| 20 | `fwupgrade_hal_download_reboot_now` | Static | valid 1 |
| 21 | `fwupgrade_hal_update_and_factoryreset` | Static | valid 1 |
| 25 / 22, 23, 24, 26, 27, 28, 29 | `fwupgrade_hal_download_install` | Dynamic | valid 1 / invalid 1-7 |

Tests declare component and level markers. Collection and all-skipped runs are
not integration passes.

## Execute through RAFT and SSH

Use an approved private rack YAML (`configs/rack.yml` is gitignored; base new
ones on [rack.example.yml](../configs/rack.example.yml)). Never
commit passwords or private keys. A command has two parts: runner settings
before `--` (mode, config, component, level, API, filters, console) and pytest
options after it (for example `-v`, `-m profile`). From the halif-test-folder root:

```bash
# run all 29 fwupgrade cases
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 --console status-only \
  -- -v

# run all registered components
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=all --level=L1 --console status-only \
  -- -v
```

--config overrides RAFT_CONFIG. Same-host requires the rack console to execute
on the same host/filesystem as the runner. Selected backends are started and
protocol-probed; only runner-owned processes are stopped. Tests run serially.
Without RAFT_CONFIG, direct pytest skips DUT cases and starts no backends.

### Case filters

Runner options go before the -- separator; pytest options go after it. The
positive case ID is the per-API/category Case_ID (see the mapping table above):

```bash
# only valid cases (11) / only invalid cases (18)
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 --valid --console status-only \
  -- -v
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 --invalid --console status-only \
  -- -v

# one API
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 \
  --api=fwupgrade_hal_set_download_url --console status-only \
  -- -v

# one specific case: download_install, invalid case 7
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 \
  --api=fwupgrade_hal_download_install --invalid --case-id=7 \
  --console full -- -v

# Excel row 7 (set_download_url, valid case 1)
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 \
  --api=fwupgrade_hal_set_download_url --valid --case-id=1 \
  --console full -- -v

# only static cases (11) / only dynamic (profile-driven) cases (18)
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 --console status-only \
  -- -v -m "not profile"
.venv/bin/python -B run_tests.py \
  --mode same-host --config configs/rack.yml \
  --component=fwupgrade --level=L1 --console status-only \
  -- -v -m profile
```

To use `--case-id`, also give `--component`, `--level`, `--api` and either
`--valid` or `--invalid` (never both). `--case-id` is the number within that
API's valid or invalid category, not a position in the whole suite.

### Remote deployment and executable paths

For another host, deploy the installed bin/lib tree there, start its server
externally, use --mode remote, and set FWUPGRADE_VL_DUT_CLI to the CLI path on
that host. FWUPGRADE_VL_SOCKET_PATH must match the remote backend socket; it is
a Unix path, not an SSH address.

Local defaults prefer build-run and fall back to install-run/bin.
FWUPGRADE_VL_SERVER overrides the same-host server path.
Existing stale/nonresponsive socket paths are not deleted automatically.

## Compact controller framework

[framework](../framework) contains seven Python files and one HTML
asset (eight files total) and is shared, unchanged, across components:

| File | Responsibility |
| --- | --- |
| `__init__.py` | Package identity; no runtime setup |
| `app_runner.py` | CLI entry point, backend ownership, and pytest lifecycle |
| `configuration.py` | Component settings, discovery, registration, and shared configuration |
| `core.py` | Marker-based selection, standard JSON/YAML profiles, and fixtures |
| `execution.py` | VL adapter, command construction, field parsing, restoration, assertions, and RaftEnvironment |
| `reporting.py` | Stage events, pytest hooks, redaction, console/XML/HTML reports |
| `raft_bridge.py` | Isolated subprocess bridge into vendored RAFT |
| `report_template.html` | Offline report presentation |

Tests import `run`, `field`, `text_field`, `restore_fields`, and `assert_equal`
from `framework.execution`.
The fwupgrade component is registered by `component/fwupgrade/settings.py`
(selector `--fwupgrade-api`, socket `/tmp/fwupgrade_vl-run.sock`). The vendored
third-party runtime and `cli` and the shared `dispatcher` sources are unchanged from the
reference setup.

## State restoration and test helpers

As in the platform tests, every test that changes state runs inside
`with restore_fields(...)`, which snapshots the GET values (status 0 required),
yields, and sets the original values back. For fwupgrade two blocks are used:
`INTERFACE_FIELDS` (integer interface) and `URL_FIELDS` (text URL and filename,
via the new backward-compatible `text_fields` option of `restore_fields`; the
platform tests, which only use integer fields, are unaffected). Tests do not
assume library defaults: they set known values from `download_fixture`, read
them back, and for the download status check only the value set documented in
`fwupgrade_hal.h`.

The remaining small helpers in the test script exist because, unlike the
platform LED test, fwupgrade has a multi-step flow (URL, interface, download,
reboot_ready) that is repeated by several tests (`_configure_download`,
`_precondition`), JSON-encoded string/NULL inputs (`_json`, `_expand` for
length-limit tokens), and a shared status plus CLI-exit check (`_verify`).

Limitation: restoring needs a readable original. If the library has no URL
configured, `fwupgrade_hal_get_download_url` fails and the snapshot is reported
as such; configure a URL on the device once before running.

The valid `fwupgrade_hal_get_download_url` and `fwupgrade_hal_get_download_interface`
cases are the exception: GET is read-only, so each runs a single GET with no
snapshot, precondition SET or restore, and only checks status 0, CLI exit 0, and
that the value(s) (url and filename, or interface) are returned.

## Reports and limitations

The runner creates a unique private logs/<run>/ folder containing
console.log, summary.html, and result.xml. HTML works offline; --console full
shows execution stages, while status-only saves detailed diagnostics to the report.
Review logs before sharing. A stand-in HAL pass does not establish physical
hardware behavior.

