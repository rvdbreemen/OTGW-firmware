#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$script_dir"

export PIP_DISABLE_PIP_VERSION_CHECK=1
export PIP_NO_INPUT=1
export PYTHONUTF8=1
export PYTHONUNBUFFERED=1

python_exe=""

use_python_if_valid() {
    candidate=$1
    if [ -x "$candidate" ] &&
        "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)' >/dev/null 2>&1; then
        python_exe=$candidate
        return 0
    fi
    return 1
}

find_base_python() {
    for candidate in python3 python; do
        if command -v "$candidate" >/dev/null 2>&1 &&
            "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)' >/dev/null 2>&1; then
            base_python=$candidate
            return 0
        fi
    done
    return 1
}

use_python_if_valid "$script_dir/.build-venv/bin/python" ||
    use_python_if_valid "$script_dir/.build-venv/bin/python3" ||
    true

if [ -z "$python_exe" ]; then
    base_python=""
    if find_base_python &&
        "$base_python" -m venv "$script_dir/.build-venv" >/dev/null 2>&1; then
        use_python_if_valid "$script_dir/.build-venv/bin/python" ||
            use_python_if_valid "$script_dir/.build-venv/bin/python3" ||
            true
    fi
fi

if [ -z "$python_exe" ]; then
    use_python_if_valid "$script_dir/.venv/bin/python" ||
        use_python_if_valid "$script_dir/.venv/bin/python3" ||
        true
fi

if [ -z "$python_exe" ]; then
    echo "ERROR: Python 3 not found. Install Python 3 or provide a working .venv." >&2
    exit 1
fi

python_bin_dir=$(dirname "$python_exe")
export PATH="$python_bin_dir:$PATH"

if ! "$python_exe" -m pip --version >/dev/null 2>&1; then
    "$python_exe" -m ensurepip --upgrade >/dev/null 2>&1
fi

install_requirements() {
    requirements_file=$1
    if [ -f "$requirements_file" ]; then
        "$python_exe" -m pip install --disable-pip-version-check --no-input -q -r "$requirements_file"
    fi
}

install_requirements "$script_dir/requirements-build.txt"
install_requirements "$script_dir/requirements.txt"

exec "$python_exe" "$script_dir/build.py" "$@"
