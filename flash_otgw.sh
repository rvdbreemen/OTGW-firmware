#!/usr/bin/env bash
# =============================================================================
#  flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware (Linux/macOS)
# -----------------------------------------------------------------------------
#  Distributed alongside merged binary releases. Downloads Espressif's
#  standalone esptool binary on first run (no Python required).
#
#  Default behaviour (no flags): auto-detects whether the target flash is
#  blank, then asks which install path to use with a recommendation.
#  Blank flash recommends the factory reset path; already-used flash
#  recommends the upgrade path that keeps WiFi credentials.
#
#  Usage:
#    ./flash_otgw.sh                     interactive chooser with recommendation
#    ./flash_otgw.sh --upgrade           force firmware-only upgrade
#    ./flash_otgw.sh --factory           force full image flash
#    ./flash_otgw.sh --erase             full clean wipe (loses everything)
#    ./flash_otgw.sh --port /dev/ttyUSB0 use specific port
#    ./flash_otgw.sh --bin <file>        use specific firmware file
#    ./flash_otgw.sh --board esp8266     force board (otherwise from filename)
#    ./flash_otgw.sh --board esp32       (Nodoshop OTGW32)
#    ./flash_otgw.sh --baud N            override baud rate
#    ./flash_otgw.sh --help              show this help
# =============================================================================

set -e
set -u
set -o pipefail 2>/dev/null || true

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
ARG_BOARD=""
ARG_BAUD=""
ARG_ERASE=0
ARG_UPGRADE=0
ARG_FACTORY=0

# ---- Help ------------------------------------------------------------------
show_help() {
    cat <<EOF
flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware

Usage:
  ./flash_otgw.sh [options]

  Mode:
  (no flag)            Interactive chooser with auto-detected default.
                       1 = factory reset, 2 = upgrade OTGW, 3 = firmware-only
  --upgrade            Force firmware-only upgrade.
  --factory            Force full image flash (keeps WiFi; resets settings).
  --erase              Full clean wipe. Erases WiFi credentials and settings.

Targeting:
  --port <dev>         Serial port (e.g. /dev/ttyUSB0, /dev/cu.usbserial-XXXX).
                       For esp32 the script falls back to USB VID/PID filter
                       (303A:1001) when no --port is given.
  --bin <file>         Firmware path. Overrides mode-based auto-detect.
  --board esp8266      Force board type.
  --board esp32        (Nodoshop OTGW32 = ESP32-S3)
  --baud N             Override baud rate (default: 460800/921600).

Other:
  --help, -h           Show this help.

The script downloads esptool ${ESPTOOL_VERSION} to ./tools/esptool/ on first run.
No Python required. On Linux it auto-escalates with sudo if the user is not
in the dialout group and the serial device is not writable.
EOF
}

# ---- Parse arguments -------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --port)     ARG_PORT="$2";  shift 2 ;;
        --bin)      ARG_BIN="$2";   shift 2 ;;
        --board)    ARG_BOARD="$2"; shift 2 ;;
        --baud)     ARG_BAUD="$2";  shift 2 ;;
        --erase)    ARG_ERASE=1;    shift ;;
        --upgrade)  ARG_UPGRADE=1;  shift ;;
        --factory)  ARG_FACTORY=1;  shift ;;
        --help|-h)  show_help; exit 0 ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit 2 ;;
    esac
done

if [ "$ARG_ERASE" = "1" ] && { [ "$ARG_UPGRADE" = "1" ] || [ "$ARG_FACTORY" = "1" ]; }; then
    err "--erase and --upgrade/--factory are mutually exclusive."
    err "        --erase wipes everything including the filesystem."
    err "        --upgrade preserves the filesystem."
    exit 2
fi

if [ "$ARG_UPGRADE" = "1" ] && [ "$ARG_FACTORY" = "1" ]; then
    err "--upgrade and --factory are mutually exclusive."
    err "        Both already select different flash layouts."
    exit 2
fi

# ---- Linux: auto-escalate to sudo if dialout permission is missing --------
# Mirrors Nodo-shop's tested approach: if the typical serial device exists,
# is not writable, and the user is not root and not in the dialout group,
# re-exec under sudo. Avoids cryptic "Permission denied: /dev/ttyUSB0" errors.
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

    if id -nG "$USER" 2>/dev/null | grep -qw "dialout"; then
        # User is in dialout but device still not writable: a logout/login is
        # likely required. Don't sudo-spam in that case.
        warn "User $USER is in 'dialout' group but cannot write to the serial"
        warn "        device. Try logging out and back in to refresh group membership."
        return 0
    fi

    info "Serial device not writable and user not in 'dialout'. Re-running with sudo..."
    exec sudo -E bash "$0" "$@"
}
maybe_sudo_relaunch "$@"

echo
echo "============================================================"
echo " OTGW Flash Tool ($(uname -s))"
echo " esptool standalone version: ${ESPTOOL_VERSION}"
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
            # Espressif ships one universal macOS asset (esptool-vX.Y.Z-macos.zip)
            # that runs on both Apple Silicon (arm64) and Intel (x86_64).
            echo "macos"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            err "Detected Windows shell ($kernel). Use flash_otgw.bat instead;"
            err "        run it from cmd.exe or PowerShell so port enumeration works."
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

# ---- Step 3: locate firmware bin -------------------------------------------
# Mode selection mirrors the .bat script:
#   --upgrade           -> *-merged.bin       (firmware-only; preserves WiFi + FS)
#   --factory           -> *-merged-full.bin  (full image; updates filesystem)
#   --erase             -> *-merged-full.bin  (full image + erase_all)
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
    if [ "$ARG_FACTORY" = "1" ] || [ "$ARG_ERASE" = "1" ]; then
        for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
            cand="$(find_first_match "$dir/OTGW-firmware-*-merged-full.bin")"
            [ -n "$cand" ] && break
        done
        if [ -z "$cand" ]; then
            err "--factory: no merged-full bin found."
            err "        Expected in: $SCRIPT_DIR"
            err "                 or: $SCRIPT_DIR/build"
            err "        Use --bin to specify a path."
            exit 1
        fi
    else
        for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
            cand="$(find_first_match "$dir/OTGW-firmware-esp32-*-merged.bin")"
            [ -n "$cand" ] && break
            cand="$(find_first_match "$dir/OTGW-firmware-esp8266-*.ino.bin")"
            [ -n "$cand" ] && break
            cand="$(find_first_match "$dir/OTGW-firmware-*-merged-full.bin")"
            [ -n "$cand" ] && break
        done
        if [ -z "$cand" ]; then
            err "No firmware-only or merged-full bin found."
            err "        Expected: OTGW-firmware-esp32-*-merged.bin"
            err "              or: OTGW-firmware-esp8266-*.ino.bin"
            err "              or: OTGW-firmware-*-merged-full.bin"
            err "        Use --bin to specify a path."
            exit 1
        fi
    fi
    echo "$cand"
}

is_flash_blank() {
    local tmpdir probe_file probe_hex
    tmpdir="$(mktemp -d 2>/dev/null || mktemp -d -t otgwflash)" || return 1
    probe_file="$tmpdir/flashprobe.bin"

    if ! "$ESPTOOL_BIN" --chip "$ESPTOOL_CHIP" $PORT_ARGS --baud "$ARG_BAUD" read-flash 0x0 0x1000 "$probe_file" >/dev/null 2>&1; then
        rm -rf "$tmpdir"
        return 1
    fi

    probe_hex="$(od -An -tx1 -N 4096 "$probe_file" 2>/dev/null | tr -d ' \r\n\t')"
    rm -rf "$tmpdir"

    case "$probe_hex" in
        "" ) return 1 ;;
        *[!fF]* ) return 1 ;;
        * ) return 0 ;;
    esac
}

PROVISIONAL_BIN_FILE="$(find_bin)"
PROVISIONAL_BIN_NAME="$(basename "$PROVISIONAL_BIN_FILE")"

# ---- Step 4: derive board from filename (or user override) -----------------
if [ -z "$ARG_BOARD" ]; then
    case "$PROVISIONAL_BIN_NAME" in
        *-esp8266-*) ARG_BOARD="esp8266" ;;
        *-esp32-*)   ARG_BOARD="esp32" ;;
    esac
fi

case "$ARG_BOARD" in
    esp8266)
        ESPTOOL_CHIP="esp8266"
        BOARD_NAME="Nodoshop OTGW WiFi (ESP8266)"
        [ -z "$ARG_BAUD" ] && ARG_BAUD="460800"
        ;;
    esp32)
        ESPTOOL_CHIP="esp32s3"
        BOARD_NAME="Nodoshop OTGW32 (ESP32-S3)"
        [ -z "$ARG_BAUD" ] && ARG_BAUD="921600"
        ;;
    "")
        err "Could not detect board from filename. Use --board esp8266 | esp32"
        exit 1
        ;;
    *)
        err "Unknown board: $ARG_BOARD (expected esp8266 or esp32)"
        exit 1
        ;;
esac

ok "Board:    $BOARD_NAME"
ok "Baud:     $ARG_BAUD"

# ---- Step 5: locate serial port --------------------------------------------
# ESP32-S3 has a fixed USB VID/PID — let esptool find it. ESP8266 boards
# vary too much in USB-serial chip choice for a clean filter; enumerate.
PORT_ARGS=""
if [ -n "$ARG_PORT" ]; then
    PORT_ARGS="--port $ARG_PORT"
    ok "Port:     $ARG_PORT"
elif [ "$ARG_BOARD" = "esp32" ]; then
    PORT_ARGS="--port-filter vid=0x303A --port-filter pid=0x1001"
    ok "Port:     auto-detect via USB VID/PID 303A:1001"
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
        err "No serial ports found. Connect your OTGW via USB."
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

if [ "$ARG_BIN" = "" ] && [ "$ARG_ERASE" = "0" ] && [ "$ARG_UPGRADE" = "0" ] && [ "$ARG_FACTORY" = "0" ]; then
    if is_flash_blank; then
        FLASH_DEFAULT_MODE="1"
        FLASH_DEFAULT_DESC="Factory reset"
    else
        FLASH_DEFAULT_MODE="2"
        FLASH_DEFAULT_DESC="Upgrade OTGW"
    fi

    if [ -t 0 ]; then
        echo
        echo "------------------------------------------------------------"
        echo " Choose flash mode:"
        echo "   [1] Factory reset"
        echo "       Fresh install of firmware and filesystem."
        echo "       Removes WiFi credentials and settings."
        echo "   [2] Upgrade OTGW"
        echo "       Refreshes firmware and filesystem."
        echo "       Keeps WiFi credentials; settings are reset."
        echo "   [3] Firmware-only upgrade"
        echo "       Updates firmware only."
        echo "       Keeps WiFi credentials and settings."
        echo "------------------------------------------------------------"
        printf "Select option [1-3] (default %s): " "$FLASH_DEFAULT_MODE"
        read -r FLASH_CHOICE
        [ -z "$FLASH_CHOICE" ] && FLASH_CHOICE="$FLASH_DEFAULT_MODE"
        case "$FLASH_CHOICE" in
            1) ARG_ERASE=1 ;;
            2) ARG_FACTORY=1 ;;
            3) ARG_UPGRADE=1 ;;
            *) err "Invalid selection."; exit 1 ;;
        esac
    else
        case "$FLASH_DEFAULT_MODE" in
            1) ARG_ERASE=1 ;;
            2) ARG_FACTORY=1 ;;
        esac
        info "Auto-selected mode: $FLASH_DEFAULT_DESC"
    fi
fi

BIN_FILE="$(find_bin)"
BIN_NAME="$(basename "$BIN_FILE")"
ok "Firmware: $BIN_NAME"

# ---- Step 6: confirm before flash ------------------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
echo "   Firmware: $BIN_NAME"
echo "   Board:    $BOARD_NAME"
echo "   Baud:     $ARG_BAUD"
if [ "$ARG_ERASE" = "1" ]; then
    echo "   Mode:     --erase  (full clean wipe)"
    echo "   Effect:   ALL data wiped: WiFi credentials, NVS, filesystem."
elif [ "$ARG_FACTORY" = "1" ]; then
    echo "   Mode:     --factory  (full image)"
    echo "   Effect:   Filesystem image is refreshed."
    echo "             Existing WiFi/settings may be replaced."
elif [ "$ARG_UPGRADE" = "1" ]; then
    echo "   Mode:     --upgrade  (firmware-only)"
    echo "   Effect:   WiFi credentials and filesystem/settings preserved."
else
    echo "   Mode:     firmware-only"
    echo "   Effect:   WiFi credentials and filesystem/settings preserved."
fi
echo "------------------------------------------------------------"
echo

# ---- Step 7: run esptool ---------------------------------------------------
# Tested baseline (Nodo-shop OT-Thing): -z compresses transfer; default-reset
# + hard-reset gives consistent strap timing on USB-JTAG boards. -e adds
# erase_all to write_flash for the --erase mode.
WRITE_FLAGS="-z"
if [ "$ARG_ERASE" = "1" ]; then
    WRITE_FLAGS="-z -e"
fi

step "Running esptool write_flash..."
"$ESPTOOL_BIN" --chip "$ESPTOOL_CHIP" $PORT_ARGS --baud "$ARG_BAUD" \
    --before default_reset --after hard_reset \
    write_flash $WRITE_FLAGS 0x0 "$BIN_FILE"

echo
echo "============================================================"
echo " Flash complete. Reset the OTGW or unplug/replug USB."
if [ "$ARG_ERASE" = "1" ]; then
    echo " After reset: connect to WiFi AP \"OTGW-AP\" to configure."
else
    echo " WiFi credentials and settings preserved; the board should rejoin"
    echo " your network automatically. Browse to http://otgw.local"
    echo " if mDNS works on your network."
fi
echo "============================================================"
