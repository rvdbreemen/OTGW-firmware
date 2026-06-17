#!/usr/bin/env sh
set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$script_dir"

export PIP_DISABLE_PIP_VERSION_CHECK=1
export PIP_NO_INPUT=1
export PYTHONUTF8=1
export PYTHONUNBUFFERED=1

bootstrap_py_dir="$script_dir/.build-python"
bootstrap_py_exe="$bootstrap_py_dir/bin/python"

# ---- Detect and clean Windows toolchain -------------------------------------
echo "============================================================"
echo " OTGW-firmware Build Script (Unix/Linux/macOS/WSL)"
echo "============================================================"
echo

# Check if Windows toolchain is present (look for .exe files in arduino/packages)
if [ -d "$script_dir/arduino/packages" ]; then
    if find "$script_dir/arduino/packages" -name "*.exe" -o -name "*.bat" | head -n 1 | grep -q .; then
        echo "[INFO] Detected Windows toolchain in arduino/packages"
        echo "[INFO] Running in Unix environment - cleaning Windows binaries..."
        rm -rf "$script_dir/arduino/packages"
        rm -rf "$script_dir/arduino/staging"
        rm -rf "$script_dir/.tmp"
        echo "[OK]   Windows toolchain removed"
        echo
    fi
fi

# ---- Bootstrap dependencies for Unix platforms ------------------------------
bootstrap_dependencies() {
    local missing=""
    local platform=""
    
    # Check for required tools
    command -v python3 >/dev/null 2>&1 || missing="$missing python3"
    command -v curl >/dev/null 2>&1 || command -v wget >/dev/null 2>&1 || missing="$missing curl"
    
    [ -z "$missing" ] && return 0
    
    echo "[INFO] Missing required tools:$missing"
    
    # Detect platform and package manager
    if [ "$(uname -s)" = "Linux" ]; then
        if command -v apt-get >/dev/null 2>&1; then
            platform="apt-get"
        elif command -v yum >/dev/null 2>&1; then
            platform="yum"
        fi
    elif [ "$(uname -s)" = "Darwin" ]; then
        if command -v brew >/dev/null 2>&1; then
            platform="brew"
        fi
    fi
    
    if [ -z "$platform" ]; then
        echo "ERROR: Cannot auto-install dependencies. Please install:$missing" >&2
        exit 1
    fi
    
    echo "[INFO] Installing dependencies using $platform..."
    
    case "$platform" in
        apt-get)
            if sudo -n apt-get update >/dev/null 2>&1 && sudo -n apt-get install -y $missing >/dev/null 2>&1; then
                echo "[OK]   Dependencies installed"
                return 0
            fi
            echo "[WARN] Need sudo password to install dependencies"
            sudo apt-get update && sudo apt-get install -y $missing || {
                echo "ERROR: Failed to install dependencies" >&2
                exit 1
            }
            ;;
        yum)
            sudo yum install -y $missing || {
                echo "ERROR: Failed to install dependencies" >&2
                exit 1
            }
            ;;
        brew)
            brew install $missing || {
                echo "ERROR: Failed to install dependencies" >&2
                exit 1
            }
            ;;
    esac
    
    echo "[OK]   Dependencies installed"
}

bootstrap_dependencies
echo

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

download_file() {
    url=$1
    out_file=$2

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url" -o "$out_file"
        return $?
    fi

    if command -v wget >/dev/null 2>&1; then
        wget -qO "$out_file" "$url"
        return $?
    fi

    return 1
}

bootstrap_portable_python() {
    if use_python_if_valid "$bootstrap_py_exe"; then
        return 0
    fi

    arch=$(uname -m 2>/dev/null || echo unknown)
    case "$arch" in
    x86_64)
        installer_name="Miniconda3-latest-Linux-x86_64.sh"
        ;;
    aarch64 | arm64)
        installer_name="Miniconda3-latest-Linux-aarch64.sh"
        ;;
    *)
        echo "ERROR: Unsupported WSL architecture for bootstrap: $arch" >&2
        return 1
        ;;
    esac

    installer_url="https://repo.anaconda.com/miniconda/$installer_name"
    installer_path="${TMPDIR:-/tmp}/$installer_name"

    echo "INFO: Python 3 not found. Bootstrapping local Python runtime..."

    if ! download_file "$installer_url" "$installer_path"; then
        echo "ERROR: Failed to download Python bootstrap installer: $installer_url" >&2
        echo "ERROR: Requires curl or wget available in WSL." >&2
        return 1
    fi

    rm -rf "$bootstrap_py_dir"
    if ! bash "$installer_path" -b -p "$bootstrap_py_dir" >/dev/null 2>&1; then
        echo "ERROR: Failed to install local Python runtime to $bootstrap_py_dir" >&2
        return 1
    fi

    if ! use_python_if_valid "$bootstrap_py_exe"; then
        echo "ERROR: Bootstrapped runtime is not a valid Python 3 interpreter" >&2
        return 1
    fi

    echo "INFO: Local Python runtime bootstrapped successfully."
    return 0
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
    bootstrap_portable_python || true
fi

if [ -z "$python_exe" ]; then
    echo "ERROR: Python 3 not found. Install Python 3 or provide a working .venv." >&2
    exit 1
fi

python_bin_dir=$(dirname "$python_exe")
export PATH="$python_bin_dir:$PATH"
export PYTHONPATH="$script_dir${PYTHONPATH:+:$PYTHONPATH}"

if ! "$python_exe" -m pip --version >/dev/null 2>&1; then
    if ! "$python_exe" -m ensurepip --upgrade >/dev/null 2>&1; then
        get_pip_path="${TMPDIR:-/tmp}/get-pip.py"
        if ! download_file "https://bootstrap.pypa.io/get-pip.py" "$get_pip_path"; then
            echo "ERROR: Failed to bootstrap pip (ensurepip unavailable and get-pip download failed)." >&2
            exit 1
        fi
        "$python_exe" "$get_pip_path" --disable-pip-version-check --no-input --quiet
    fi
fi

install_requirements() {
    requirements_file=$1
    if [ -f "$requirements_file" ]; then
        "$python_exe" -m pip install --disable-pip-version-check --no-input -q -r "$requirements_file"
    fi
}

install_requirements "$script_dir/requirements-build.txt"
install_requirements "$script_dir/requirements.txt"

# WSL + Windows .pio cache guard: Windows PlatformIO embeds absolute Windows paths (D:\...)
# in its build cache. When WSL runs PlatformIO on that cache, GCC gets invalid include paths
# and fails with "No such file or directory" for headers that exist on Windows but not at
# Linux paths. Auto-clean when WSL is detected and a Windows-compiled .pio/build/ is present.
if grep -qi "microsoft\|wsl" /proc/version 2>/dev/null; then
    pio_build_dir="$script_dir/.pio/build"
    needs_clean=0
    if [ -d "$pio_build_dir" ]; then
        # A Windows build leaves .rsp/.d files with absolute Windows paths (e.g. D:/Users/...).
        # Detecting a Windows drive letter pattern [A-Z]:/ is a reliable signal.
        if find "$pio_build_dir" \( -name "*.rsp" -o -name "*.d" \) -print0 2>/dev/null \
               | xargs -0 grep -lI "[A-Z]:/" 2>/dev/null \
               | grep -q .; then
            needs_clean=1
        fi
    fi
    if [ "$needs_clean" -eq 1 ]; then
        echo "WSL detected: stale Windows .pio/build cache found — running clean first..." >&2
        "$python_exe" -m platformio run --target clean
    fi
fi

exec "$python_exe" "$script_dir/build.py" "$@"
