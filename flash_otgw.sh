#!/usr/bin/env bash
# =============================================================================
#  flash_otgw.sh - Zero-install flash tool for OTGW-firmware (Linux/macOS)
# -----------------------------------------------------------------------------
#  Distributed with each OTGW-firmware release. Downloads Espressif's
#  standalone esptool binary on first run — no Python or pip required.
#
#  Flashes the two release binaries onto an ESP8266:
#    • OTGW-firmware-*.ino.bin      (firmware)   at 0x0
#    • OTGW-firmware*.littlefs.bin  (filesystem) at 0x200000
#
#  Serial port is auto-detected. If multiple ports are found the first one
#  is used; supply --port to override.
#
#  Usage:
#    ./flash_otgw.sh                         # auto-detect everything
#    ./flash_otgw.sh --port /dev/ttyUSB0     # explicit port
#    ./flash_otgw.sh --baud 115200           # override baud rate
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

# ---- Terminal colours -------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    C_GREEN=$'\033[0;32m'; C_RED=$'\033[0;31m'; C_YELLOW=$'\033[0;33m'
    C_CYAN=$'\033[0;36m';  C_BOLD=$'\033[1m';   C_RESET=$'\033[0m'
else
    C_GREEN=""; C_RED=""; C_YELLOW=""; C_CYAN=""; C_BOLD=""; C_RESET=""
fi

ok()   { printf "%s[OK]%s    %s\n" "$C_GREEN"  "$C_RESET" "$*"; }
info() { printf "%s[INFO]%s  %s\n" "$C_CYAN"   "$C_RESET" "$*"; }
warn() { printf "%s[WARN]%s  %s\n" "$C_YELLOW" "$C_RESET" "$*"; }
err()  { printf "%s[ERROR]%s %s\n" "$C_RED"    "$C_RESET" "$*" >&2; }
step() { printf "\n%s[STEP]%s  %s\n" "$C_BOLD$C_CYAN" "$C_RESET" "$*"; }

# ---- Defaults ---------------------------------------------------------------
ARG_PORT=""
ARG_BAUD="460800"

# ---- Help -------------------------------------------------------------------
show_help() {
    cat <<EOF
flash_otgw.sh - Zero-install flash tool for OTGW-firmware (ESP8266)

Usage:
  ./flash_otgw.sh [options]

Flashes both firmware and filesystem binaries found next to this script:
  OTGW-firmware-*.ino.bin       firmware   → 0x0
  OTGW-firmware*.littlefs.bin   filesystem → 0x200000

The serial port is auto-detected. If multiple ports are present, the first
one found is used; specify --port to override.

Options:
  --port <dev>    Serial port (e.g. /dev/ttyUSB0, /dev/cu.usbserial-XXXX).
  --baud N        Override baud rate (default: ${ARG_BAUD}).
  --help, -h      Show this help.

First run:
  Downloads esptool ${ESPTOOL_VERSION} to ./tools/esptool/ — no Python required.
  On Linux, auto-escalates with sudo if the user lacks serial port access.

After flashing:
  The device opens a WiFi AP named "OTGW-<MAC-address>".
  Connect and browse to http://192.168.4.1 to configure WiFi.
EOF
}

# ---- Parse arguments --------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --port)    ARG_PORT="$2"; shift 2 ;;
        --baud)    ARG_BAUD="$2"; shift 2 ;;
        --help|-h) show_help; exit 0 ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit 2 ;;
    esac
done

# ---- Linux: auto-escalate to sudo when serial device is not writable --------
maybe_sudo_relaunch() {
    [ "$(uname -s)" = "Linux" ] || return 0
    [ "${EUID:-$(id -u)}" -ne 0 ] || return 0   # already root

    local need_sudo=0
    for d in /dev/ttyUSB0 /dev/ttyACM0 /dev/ttyUSB1 /dev/ttyACM1; do
        [ -e "$d" ] || continue
        [ ! -w "$d" ] && need_sudo=1
    done
    [ "$need_sudo" = "1" ] || return 0

    # Check for common serial groups: dialout (Debian/Ubuntu), uucp (Arch)
    local serial_groups="dialout uucp"
    for grp in $serial_groups; do
        if id -nG "$USER" 2>/dev/null | grep -qw "$grp"; then
            warn "User $USER is in '$grp' but cannot write to the serial device."
            warn "Try logging out and back in to refresh group membership."
            return 0
        fi
    done

    info "Serial device not writable — re-running with sudo..."
    exec sudo -E bash "$0" "${ORIGINAL_ARGS[@]}"
}
maybe_sudo_relaunch

echo
echo "============================================================"
echo " OTGW Flash Tool  ($(uname -s))"
echo " Target: Nodoshop OTGW WiFi module (ESP8266)"
echo " esptool: ${ESPTOOL_VERSION}"
echo "============================================================"
echo

# ---- Step 1: detect host platform -------------------------------------------
detect_platform() {
    local kernel arch
    kernel="$(uname -s)"
    arch="$(uname -m)"
    case "$kernel" in
        Linux)
            case "$arch" in
                x86_64|amd64)       echo "linux-amd64" ;;
                aarch64|arm64)      echo "linux-arm64" ;;
                armv7l|armv7|armhf) echo "linux-arm32" ;;
                *) err "Unsupported Linux arch: $arch"; return 1 ;;
            esac ;;
        Darwin)
            echo "macos" ;;
        MINGW*|MSYS*|CYGWIN*)
            err "Detected Windows shell. Use flash_otgw.bat from cmd.exe or PowerShell."
            return 1 ;;
        *) err "Unsupported OS: $kernel"; return 1 ;;
    esac
}

# ---- Step 2: ensure esptool is present --------------------------------------
ensure_esptool() {
    if [ -x "$ESPTOOL_BIN" ]; then
        ok "esptool: $ESPTOOL_BIN"
        return 0
    fi

    local platform_tag asset_name url zip_path
    platform_tag="$(detect_platform)" || exit 1
    asset_name="esptool-${ESPTOOL_VERSION}-${platform_tag}.zip"
    url="https://github.com/espressif/esptool/releases/download/${ESPTOOL_VERSION}/${asset_name}"
    zip_path="${TOOLS_DIR}/${asset_name}"

    info "Downloading esptool from Espressif..."
    mkdir -p "$TOOLS_DIR"

    if command -v curl >/dev/null 2>&1; then
        curl -fSL --retry 2 -o "$zip_path" "$url" || { err "Download failed (curl)."; exit 1; }
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$zip_path" "$url" || { err "Download failed (wget)."; exit 1; }
    else
        err "Neither curl nor wget is available. Install one and re-run."; exit 1
    fi

    command -v unzip >/dev/null 2>&1 || { err "'unzip' is required. Install it and re-run."; exit 1; }

    info "Extracting esptool..."
    unzip -q -o "$zip_path" -d "$TOOLS_DIR"

    local found
    found="$(find "$TOOLS_DIR" -maxdepth 4 -type f -name esptool 2>/dev/null | head -n 1)"
    [ -n "$found" ] || { err "Extracted archive did not contain an esptool binary."; exit 1; }

    cp "$found" "$ESPTOOL_BIN"
    chmod +x "$ESPTOOL_BIN"
    rm -f "$zip_path"
    ok "esptool installed: $ESPTOOL_BIN"
}

ensure_esptool

# ---- Step 3: locate firmware and filesystem binaries ------------------------
find_first_match() {
    # Intentional glob expansion: $1 contains a wildcard pattern.
    for f in $1; do
        [ -f "$f" ] && echo "$f"
    done | sort | head -n 1
}

FW_FILE=""
FS_FILE=""

for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
    [ -d "$dir" ] || continue
    [ -z "$FW_FILE" ] && FW_FILE="$(find_first_match "$dir/OTGW-firmware-*.ino.bin")"
    [ -z "$FS_FILE" ] && FS_FILE="$(find_first_match "$dir/OTGW-firmware*.littlefs.bin")"
done

if [ -z "$FW_FILE" ]; then
    err "Firmware binary not found."
    err "        Expected: OTGW-firmware-*.ino.bin"
    err "        Download the firmware from the GitHub release page and place it"
    err "        in the same directory as this script."
    exit 1
fi

if [ -z "$FS_FILE" ]; then
    err "Filesystem binary not found."
    err "        Expected: OTGW-firmware*.littlefs.bin"
    err "        Download the filesystem binary from the GitHub release page and"
    err "        place it in the same directory as this script."
    exit 1
fi

ok "Firmware:   $(basename "$FW_FILE")"
ok "Filesystem: $(basename "$FS_FILE")"
ok "Baud:       $ARG_BAUD"

# ---- Step 4: auto-detect serial port ----------------------------------------
if [ -n "$ARG_PORT" ]; then
    ok "Port:       $ARG_PORT (specified)"
else
    list_ports() {
        case "$(uname -s)" in
            Linux)  ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null ;;
            Darwin) ls -1 /dev/cu.usbserial-* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART \
                         /dev/cu.wchusbserial* 2>/dev/null ;;
        esac
    }

    PORTS=()
    while IFS= read -r p; do
        [ -n "$p" ] && PORTS+=("$p")
    done < <(list_ports | sort)

    if [ ${#PORTS[@]} -eq 0 ]; then
        err "No serial ports found. Connect your OTGW via USB and try again."
        err "        Linux:  expect /dev/ttyUSB* or /dev/ttyACM*"
        err "        macOS:  expect /dev/cu.usbserial-* or /dev/cu.usbmodem*"
        exit 1
    fi

    ARG_PORT="${PORTS[0]}"
    if [ ${#PORTS[@]} -eq 1 ]; then
        ok "Port:       $ARG_PORT (auto-detected)"
    else
        info "Multiple ports found — using first: $ARG_PORT"
        info "Other ports: ${PORTS[*]:1}"
        info "Use --port to select a different one."
    fi
fi

# ---- Step 5: summary --------------------------------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
printf "   Firmware:   %s\n" "$(basename "$FW_FILE")"
printf "   Filesystem: %s\n" "$(basename "$FS_FILE")"
echo "   Port:       $ARG_PORT"
echo "   Baud:       $ARG_BAUD"
echo ""
echo "   Erases flash, then writes firmware at 0x0"
echo "   and filesystem at 0x200000."
echo "------------------------------------------------------------"
echo

# ---- Step 6: flash ----------------------------------------------------------
step "Flashing firmware and filesystem..."

"$ESPTOOL_BIN" --chip esp8266 --port "$ARG_PORT" --baud "$ARG_BAUD" \
    --before default_reset --after hard_reset \
    write_flash -z --erase-all \
    0x0       "$FW_FILE" \
    0x200000  "$FS_FILE"

echo
echo "============================================================"
echo " Flash complete."
echo " Connect to WiFi AP 'OTGW-<MAC-address>' and browse to"
echo " http://192.168.4.1 to configure WiFi settings."
echo "============================================================"
