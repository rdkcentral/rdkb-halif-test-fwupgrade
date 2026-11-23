#!/usr/bin/env bash

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

#!/usr/bin/env bash
# Usage: bash build.sh [additional CMake configuration arguments]
# Example real HAL build:
#   bash build.sh -DFWUPGRADE_HAL_LIB=/absolute/path/to/libfwupgrade_hal.so
# Prerequisites: Git, CMake >= 3.13, make (or another CMake generator),
# a C compiler, and Python >= 3.10 with venv/pip and development headers.
# Python 3.10 is the verified environment for the current RAFT requirements.
# Debian/Ubuntu prerequisites can be installed by an administrator with:
#   apt-get install -y git cmake build-essential python3-dev python3-venv
set -Eeuo pipefail
trap 'printf "Setup failed at line %s.\n" "$LINENO" >&2' ERR

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

export GIT_TERMINAL_PROMPT=0
export PIP_NO_INPUT=1
export PIP_DISABLE_PIP_VERSION_CHECK=1
export PYTHONDONTWRITEBYTECODE=1

for tool in git cmake; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Missing prerequisite: %s. Install it and rerun.\n' "$tool" >&2
        exit 1
    fi
done

if [[ -z "${PYTHON:-}" ]]; then
    if command -v python3.10 >/dev/null 2>&1; then
        PYTHON=python3.10
    else
        PYTHON=python3
    fi
fi
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    printf 'Python not found: %s. Set PYTHON to a Python 3.10+ executable.\n' "$PYTHON" >&2
    exit 1
fi
"$PYTHON" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else "Python 3.10+ is required")'

if [[ ! -e "$ROOT/.venv" ]]; then
    printf '\nCreating .venv...\n'
    "$PYTHON" -m venv "$ROOT/.venv"
elif [[ ! -x "$ROOT/.venv/bin/python" ]]; then
    printf 'Existing .venv is invalid; repair it manually before rerunning.\n' >&2
    exit 1
fi
VENV_PYTHON="$ROOT/.venv/bin/python"
"$VENV_PYTHON" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else "Existing .venv requires Python 3.10+")'

RAFT_ROOT="$ROOT/third_party/python_raft"
RAFT_URL="https://github.com/rdkcentral/python_raft.git"
if [[ ! -e "$RAFT_ROOT" ]]; then
    printf '\nCloning Python RAFT...\n'
    mkdir -p "$ROOT/third_party"
    git clone "$RAFT_URL" "$RAFT_ROOT"
elif [[ ! -d "$RAFT_ROOT/.git" && ! -f "$RAFT_ROOT/.git" ]]; then
    printf 'Existing %s is not a Git checkout; refusing to overwrite it.\n' "$RAFT_ROOT" >&2
    exit 1
else
    printf '\nReusing existing Python RAFT checkout (no pull/reset).\n'
fi
if [[ ! -f "$RAFT_ROOT/requirements.txt" ]]; then
    printf 'RAFT requirements.txt is missing in %s.\n' "$RAFT_ROOT" >&2
    exit 1
fi
printf 'RAFT revision: '
git -C "$RAFT_ROOT" rev-parse HEAD

printf '\nInstalling Python dependencies...\n'
"$VENV_PYTHON" -m pip install --upgrade pip setuptools wheel
"$VENV_PYTHON" -m pip install -r "$RAFT_ROOT/requirements.txt" 'pytest>=7,<10'
"$VENV_PYTHON" -m pip check

printf '\nChecking RAFT import...\n'
(
    cd "$RAFT_ROOT"
    "$VENV_PYTHON" -c 'from framework.core.testControl import testController; print("RAFT testController import: OK")'
)

printf '\nConfiguring and building native binaries...\n'
# build-run is searched by configs/settings.py. An empty FWUPGRADE_HAL_LIB builds
# the stand-in HAL; pass -DFWUPGRADE_HAL_LIB=... to test a vendor library instead.
cmake -S "$ROOT" -B "$ROOT/build-run" "$@"
cmake --build "$ROOT/build-run" --parallel "${BUILD_JOBS:-2}"

printf '\nSetup/build complete.\n'
printf 'Python: %s\n' "$VENV_PYTHON"
printf 'CLI:    %s/build-run/vl_cli\n' "$ROOT"
printf 'Server: %s/build-run/fwupgrade_vl\n' "$ROOT"
printf '\nActivate with: source "%s/.venv/bin/activate"\n' "$ROOT"
printf 'DUT/rack configuration and SSH credentials must still be supplied separately.\n'
printf 'No services or hardware tests were started. Hardware L2 requires a real HAL,\n'
printf 'and explicit hardware opt-in.\n'