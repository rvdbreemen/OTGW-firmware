#!/bin/sh
# Launcher for wipe_ha_discovery.py (Linux/macOS).
# Finds a Python 3 interpreter and, if none is present, attempts to install
# one with the system package manager before running the wiper.
# Set WIPE_HA_NO_AUTOINSTALL=1 to disable the auto-install step.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PY_SCRIPT="$SCRIPT_DIR/wipe_ha_discovery.py"

find_python() {
    for c in python3 python; do
        if command -v "$c" >/dev/null 2>&1 &&
           "$c" -c 'import sys; raise SystemExit(0 if sys.version_info[0] == 3 else 1)' >/dev/null 2>&1; then
            echo "$c"
            return 0
        fi
    done
    return 1
}

sudo_prefix() {
    if [ "$(id -u)" -eq 0 ]; then
        echo ""
    elif command -v sudo >/dev/null 2>&1; then
        echo "sudo"
    else
        echo ""
    fi
}

auto_install() {
    SUDO=$(sudo_prefix)
    if [ "$(uname -s)" = "Darwin" ]; then
        if command -v brew >/dev/null 2>&1; then
            echo "Installing Python 3 via Homebrew ..."
            brew install python3 || return 1
        else
            return 1
        fi
    elif command -v apt-get >/dev/null 2>&1; then
        echo "Installing Python 3 via apt-get ..."
        $SUDO apt-get update && $SUDO apt-get install -y python3 || return 1
    elif command -v dnf >/dev/null 2>&1; then
        echo "Installing Python 3 via dnf ..."
        $SUDO dnf install -y python3 || return 1
    elif command -v yum >/dev/null 2>&1; then
        echo "Installing Python 3 via yum ..."
        $SUDO yum install -y python3 || return 1
    elif command -v pacman >/dev/null 2>&1; then
        echo "Installing Python 3 via pacman ..."
        $SUDO pacman -Sy --noconfirm python || return 1
    elif command -v zypper >/dev/null 2>&1; then
        echo "Installing Python 3 via zypper ..."
        $SUDO zypper install -y python3 || return 1
    elif command -v apk >/dev/null 2>&1; then
        echo "Installing Python 3 via apk ..."
        $SUDO apk add python3 || return 1
    else
        return 1
    fi
    return 0
}

if PY=$(find_python); then
    :
else
    if [ "${WIPE_HA_NO_AUTOINSTALL:-0}" = "1" ]; then
        echo "Python 3 not found and auto-install disabled." >&2
        echo "Install Python 3 from https://www.python.org/downloads/ and re-run." >&2
        exit 1
    fi
    echo "Python 3 not found. Attempting to install it ..."
    if auto_install && PY=$(find_python); then
        echo "Python 3 installed successfully."
    else
        echo "Could not auto-install Python 3 on this system." >&2
        echo "Install it manually from https://www.python.org/downloads/ (or via your package manager) and re-run." >&2
        exit 1
    fi
fi

exec "$PY" "$PY_SCRIPT" "$@"
