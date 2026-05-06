#!/usr/bin/env bash
# =============================================================================
#  flash_otgw.sh - Self-contained flash tool for OTGW-firmware (Linux/macOS)
# -----------------------------------------------------------------------------
#  Distributed alongside release binaries. Downloads Espressif's standalone
#  esptool binary on first run — no Python required.
#
#  Default behaviour: factory reset. The script erases flash and writes the
#  merged binary containing both firmware and filesystem.
#
#  Usage:
#    ./flash_otgw.sh
#    ./flash_otgw.sh --port /dev/ttyUSB0
#    ./flash_otgw.sh --bin OTGW-firmware-1.5.0-merged.bin
#    ./flash_otgw.sh --baud N
#    ./flash_otgw.sh --help
#
#  Supported hardware: Nodoshop OTGW with Wemos D1 mini / NodeMCU (ESP8266)
# =============================================================================

set -e
set -u
set -o pipefail 2>/dev/null || true

ORIGINAL_ARGS=("$@")
ESPTOOL_VERSION="v4.8.1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="${SCRIPT_DIR}/tools/esptool"
ESPTOOL_BIN="${TOOLS_DIR}/esptool"

# ---- Terminal colours ------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_GREEN=$'\033[0;32m'
    C_RED=$'\033[0;31m'
    C_YELLOW=$'\033[0;33m'
    C_CYAN=$'\033[0;36m'
    C_BOLD=$'\033[1m'
    C_RESET=$'\033[0m'
else
    C_GREEN=""; C_RED=""; C_YELLOW=""; C_CYAN=""; C_BOLD=""; C_RESET=""
fi

ok()    { printf "%s[OK]%s    %s\n" "$C_GREEN"  "$C_RESET" "$*"; }
info()  { printf "%s[INFO]%s  %s\n" "$C_CYAN"   "$C_RESET" "$*"; }
warn()  { printf "%s[WARN]%s  %s\n" "$C_YELLOW" "$C_RESET" "$*"; }
err()   { printf "%s[ERROR]%s %s\n" "$C_RED"    "$C_RESET" "$*" >&2; }
step()  { printf "\n%s[STEP]%s  %s\n" "$C_BOLD$C_CYAN" "$C_RESET" "$*"; }

# ---- Defaults --------------------------------------------------------------
ARG_PORT=""
ARG_BIN=""
ARG_BAUD="460800"

# ---- Help ------------------------------------------------------------------
show_help() {
    cat <<EOF
flash_otgw.sh - Self-contained flash tool for OTGW-firmware (ESP8266)

Usage:
  ./flash_otgw.sh [options]

Default:
  Factory reset: erase flash, then write firmware and filesystem from the
  merged binary. WiFi credentials and settings are removed.

Options:
  --port <dev>    Serial port (e.g. /dev/ttyUSB0, /dev/cu.usbserial-XXXX).
                  Auto-detected when only one port is present.
  --bin <file>    Path to the merged binary (OTGW-firmware-*-merged.bin).
                  Auto-detected when a single match is found next to this script
                  or in the build/ subdirectory.
  --baud N        Override baud rate (default: ${ARG_BAUD}).
  --help, -h      Show this help.

First run:
  Downloads esptool ${ESPTOOL_VERSION} to ./tools/esptool/ — no Python required.
  On Linux, auto-escalates with sudo if the user is not in the dialout group.

After flashing:
  The device resets and opens a WiFi AP named "OTGW-<MAC-address>".
  Connect to it and browse to http://192.168.4.1 to configure WiFi.
EOF
}

# ---- Parse arguments -------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --port)    ARG_PORT="$2";  shift 2 ;;
        --bin)     ARG_BIN="$2";   shift 2 ;;
        --baud)    ARG_BAUD="$2";  shift 2 ;;
        --help|-h) show_help; exit 0 ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit 2 ;;
    esac
done

# ---- Linux: auto-escalate to sudo if dialout permission is missing --------
# If the typical serial device exists, is not writable, and the user is not
# root and not in the dialout group, re-exec under sudo to avoid cryptic
# "Permission denied: /dev/ttyUSB0" errors.
maybe_sudo_relaunch() {
    [ "$(uname -s)" = "Linux" ] || return 0
    [ "${EUID:-$(id -u)}" -ne 0 ] || return 0

    local need_sudo=0
    for d in /dev/ttyUSB0 /dev/ttyACM0 /dev/ttyUSB1 /dev/ttyACM1; do
        [ -e "$d" ] || continue
        if [ ! -w "$d" ]; then
            need_sudo=1
        fi
    done

    [ "$need_sudo" = "1" ] || return 0

    # Check for common serial access groups: 'dialout' (Debian/Ubuntu/Raspbian),
    # 'uucp' (Arch, openSUSE). Other distros may use 'wheel' or 'tty'.
    local serial_groups="dialout uucp"
    for grp in $serial_groups; do
        if id -nG "$USER" 2>/dev/null | grep -qw "$grp"; then
            # User is in a serial group but device still not writable:
            # a logout/login is likely required to refresh group membership.
            warn "User $USER is in the '$grp' group but cannot write to the serial device."
            warn "Try logging out and back in to refresh group membership."
            return 0
        fi
    done

    info "Serial device not writable and user not in a serial group. Re-running with sudo..."
    exec sudo -E bash "$0" "${ORIGINAL_ARGS[@]}"
}
maybe_sudo_relaunch

echo
echo "============================================================"
echo " OTGW Flash Tool ($(uname -s))"
echo " Target: Nodoshop OTGW WiFi module (ESP8266)"
echo " esptool standalone version: ${ESPTOOL_VERSION}"
echo " Mode: factory reset (erase flash, write firmware + filesystem)"
echo "============================================================"
echo

# ---- Step 1: detect host platform and pick esptool asset -------------------
detect_platform() {
    local kernel arch
    kernel="$(uname -s)"
    arch="$(uname -m)"
    case "$kernel" in
        Linux)
            case "$arch" in
                x86_64|amd64)        echo "linux-amd64" ;;
                aarch64|arm64)       echo "linux-arm64" ;;
                armv7l|armv7|armhf)  echo "linux-arm32" ;;
                *) err "Unsupported Linux arch: $arch"; return 1 ;;
            esac
            ;;
        Darwin)
            # Espressif ships one universal macOS asset that runs on both
            # Apple Silicon (arm64) and Intel (x86_64).
            echo "macos"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            err "Detected Windows shell ($kernel). Use flash_otgw.bat instead;"
            err "        run it from cmd.exe or PowerShell."
            return 1
            ;;
        *) err "Unsupported OS: $kernel"; return 1 ;;
    esac
}

# ---- Step 2: ensure esptool is present -------------------------------------
ensure_esptool() {
    if [ -x "$ESPTOOL_BIN" ]; then
        ok "esptool found: $ESPTOOL_BIN"
        return 0
    fi

    local platform_tag asset_name url zip_path
    platform_tag="$(detect_platform)" || exit 1
    asset_name="esptool-${ESPTOOL_VERSION}-${platform_tag}.zip"
    url="https://github.com/espressif/esptool/releases/download/${ESPTOOL_VERSION}/${asset_name}"
    zip_path="${TOOLS_DIR}/${asset_name}"

    info "esptool not found. Downloading from Espressif..."
    info "URL: $url"
    mkdir -p "$TOOLS_DIR"

    if command -v curl >/dev/null 2>&1; then
        curl -fSL --retry 2 -o "$zip_path" "$url" || { err "Download failed (curl)."; exit 1; }
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$zip_path" "$url" || { err "Download failed (wget)."; exit 1; }
    else
        err "Neither curl nor wget is available. Install one and re-run."
        exit 1
    fi

    if ! command -v unzip >/dev/null 2>&1; then
        err "'unzip' is required to extract the esptool archive. Install it and re-run."
        exit 1
    fi

    info "Extracting esptool..."
    unzip -q -o "$zip_path" -d "$TOOLS_DIR"

    local found
    found="$(find "$TOOLS_DIR" -maxdepth 4 -type f -name esptool 2>/dev/null | head -n 1)"
    if [ -z "$found" ]; then
        err "Extracted archive did not contain an esptool binary."
        exit 1
    fi

    cp "$found" "$ESPTOOL_BIN"
    chmod +x "$ESPTOOL_BIN"
    rm -f "$zip_path"
    ok "esptool installed: $ESPTOOL_BIN"
}

ensure_esptool

# ---- Step 3: locate merged firmware binary ---------------------------------
find_first_match() {
    local pattern="$1"
    # shellcheck disable=SC2012  # ls -1 sort order is fine here
    ls -1 $pattern 2>/dev/null | head -n 1
}

find_bin() {
    if [ -n "$ARG_BIN" ]; then
        if [ ! -f "$ARG_BIN" ]; then
            err "Specified --bin file does not exist: $ARG_BIN"
            exit 1
        fi
        echo "$ARG_BIN"
        return 0
    fi

    local cand=""
    for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
        cand="$(find_first_match "$dir/OTGW-firmware-*-merged.bin")"
        [ -n "$cand" ] && break
    done
    if [ -z "$cand" ]; then
        err "No merged firmware binary found."
        err "        Expected: OTGW-firmware-*-merged.bin"
        err "        Search locations: $SCRIPT_DIR"
        err "                      or: $SCRIPT_DIR/build"
        err "        Use --bin to specify a path, or build the merged binary with:"
        err "          python3 build.py --merged"
        err "        (builds firmware + filesystem and creates the merged binary)"
        exit 1
    fi
    echo "$cand"
}

BIN_FILE="$(find_bin)"
BIN_NAME="$(basename "$BIN_FILE")"
ok "Firmware: $BIN_NAME"
ok "Baud:     $ARG_BAUD"

# ---- Step 4: locate serial port --------------------------------------------
PORT_ARGS=""
if [ -n "$ARG_PORT" ]; then
    PORT_ARGS="--port $ARG_PORT"
    ok "Port:     $ARG_PORT"
else
    list_ports() {
        case "$(uname -s)" in
            Linux)
                ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
                ;;
            Darwin)
                ls -1 /dev/cu.usbserial-* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART /dev/cu.wchusbserial* 2>/dev/null
                ;;
        esac
    }

    info "Detecting available serial ports..."
    PORTS=()
    while IFS= read -r p; do
        [ -n "$p" ] && PORTS+=("$p")
    done < <(list_ports)

    if [ ${#PORTS[@]} -eq 0 ]; then
        err "No serial ports found. Connect your OTGW via USB and try again."
        err "        Linux:  expect /dev/ttyUSB* or /dev/ttyACM*"
        err "        macOS:  expect /dev/cu.usbserial-* or /dev/cu.usbmodem*"
        exit 1
    fi

    if [ ${#PORTS[@]} -eq 1 ]; then
        ARG_PORT="${PORTS[0]}"
        ok "Auto-selected port: $ARG_PORT"
    else
        echo
        i=1
        for p in "${PORTS[@]}"; do
            printf "  [%d] %s\n" "$i" "$p"
            i=$((i+1))
        done
        echo
        printf "Select port number [1-%d]: " "${#PORTS[@]}"
        read -r CHOICE
        if ! echo "$CHOICE" | grep -qE '^[0-9]+$' || [ "$CHOICE" -lt 1 ] || [ "$CHOICE" -gt "${#PORTS[@]}" ]; then
            err "Invalid port selection."
            exit 1
        fi
        ARG_PORT="${PORTS[$((CHOICE-1))]}"
    fi
    PORT_ARGS="--port $ARG_PORT"
    ok "Port:     $ARG_PORT"
fi

# ---- Step 5: show selected flash action ------------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
echo "   Firmware: $BIN_NAME"
echo "   Baud:     $ARG_BAUD"
echo "   Mode:     factory reset"
echo "   Effect:   Erases flash, then writes firmware and filesystem."
echo "             WiFi credentials and settings will be removed."
echo "------------------------------------------------------------"
echo

# ---- Step 6: run esptool ---------------------------------------------------
step "Running esptool write_flash..."
"$ESPTOOL_BIN" --chip esp8266 $PORT_ARGS --baud "$ARG_BAUD" \
    --before default_reset --after hard_reset \
    write_flash -z -e 0x0 "$BIN_FILE"

echo
echo "============================================================"
echo " Flash complete."
echo " After reset: connect to WiFi AP 'OTGW-<MAC-address>' and"
echo " browse to http://192.168.4.1 to configure WiFi settings."
echo "============================================================"
