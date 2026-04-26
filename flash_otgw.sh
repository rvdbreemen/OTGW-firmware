#!/usr/bin/env bash
# =============================================================================
#  flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware (Linux/macOS)
# -----------------------------------------------------------------------------
#  Distributed alongside merged binary releases. Downloads Espressif's
#  standalone esptool binary on first run (no Python required).
#
#  Usage:
#    ./flash_otgw.sh                     auto-detect bin and serial port
#    ./flash_otgw.sh --port /dev/ttyUSB0 use specific port
#    ./flash_otgw.sh --bin <file>        use specific firmware file
#    ./flash_otgw.sh --board esp8266     force board (otherwise from filename)
#    ./flash_otgw.sh --board esp32       (Nodoshop OTGW32)
#    ./flash_otgw.sh --baud 921600       override baud rate
#    ./flash_otgw.sh --no-erase          skip erase_flash before write_flash
#    ./flash_otgw.sh --yes               skip all confirmation prompts
#    ./flash_otgw.sh --help              show this help
# =============================================================================

set -e
set -u
# pipefail also works in bash 3.2
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

ok()    { printf "%s[OK]%s    %s\n" "$C_GREEN" "$C_RESET" "$*"; }
info()  { printf "%s[INFO]%s  %s\n" "$C_CYAN"  "$C_RESET" "$*"; }
warn()  { printf "%s[WARN]%s  %s\n" "$C_YELLOW" "$C_RESET" "$*"; }
err()   { printf "%s[ERROR]%s %s\n" "$C_RED"   "$C_RESET" "$*" >&2; }
step()  { printf "\n%s[STEP]%s  %s\n" "$C_BOLD$C_CYAN" "$C_RESET" "$*"; }

# ---- Defaults --------------------------------------------------------------
ARG_PORT=""
ARG_BIN=""
ARG_BOARD=""
ARG_BAUD=""
ARG_NO_ERASE=0
ARG_YES=0

# ---- Help ------------------------------------------------------------------
show_help() {
    cat <<EOF
flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware

Usage:
  ./flash_otgw.sh [options]

Options:
  --port <dev>         Serial port (e.g. /dev/ttyUSB0, /dev/cu.usbserial-XXXX)
  --bin <file>         Firmware path (auto-detect *-merged-full.bin if omitted)
  --board esp8266      Force board type
  --board esp32        (Nodoshop OTGW32 = ESP32-S3)
  --baud <num>         Override baud rate (default: 460800 esp8266 / 921600 esp32)
  --no-erase           Skip erase_flash before write_flash (preserves data, risky)
  --yes, -y            Skip all confirmation prompts (for automation)
  --help, -h           Show this help

The script downloads esptool ${ESPTOOL_VERSION} to ./tools/esptool/ on first run.
No Python required.
EOF
}

# ---- Parse arguments -------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --port)     ARG_PORT="$2";  shift 2 ;;
        --bin)      ARG_BIN="$2";   shift 2 ;;
        --board)    ARG_BOARD="$2"; shift 2 ;;
        --baud)     ARG_BAUD="$2";  shift 2 ;;
        --no-erase) ARG_NO_ERASE=1; shift ;;
        --yes|-y)   ARG_YES=1;      shift ;;
        --help|-h)  show_help; exit 0 ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit 2 ;;
    esac
done

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
        if ! curl -fSL --retry 2 -o "$zip_path" "$url"; then
            err "Download failed (curl)."
            exit 1
        fi
    elif command -v wget >/dev/null 2>&1; then
        if ! wget -q -O "$zip_path" "$url"; then
            err "Download failed (wget)."
            exit 1
        fi
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

    # Find the extracted esptool binary (lives in a versioned subdir)
    local found
    found="$(find "$TOOLS_DIR" -maxdepth 4 -type f -name esptool -perm -u+x 2>/dev/null | head -n 1)"
    if [ -z "$found" ]; then
        # Fallback: search by name only and chmod ourselves
        found="$(find "$TOOLS_DIR" -maxdepth 4 -type f -name esptool 2>/dev/null | head -n 1)"
    fi

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
find_bin() {
    if [ -n "$ARG_BIN" ]; then
        if [ ! -f "$ARG_BIN" ]; then
            err "Specified --bin file does not exist: $ARG_BIN"
            exit 1
        fi
        echo "$ARG_BIN"
        return 0
    fi

    local candidate
    # Search order: same dir as script, then ./build/
    for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
        # shellcheck disable=SC2012  # ls -1 sorts alphabetically; we want first match
        candidate="$(ls -1 "$dir"/OTGW-firmware-*-merged-full.bin 2>/dev/null | head -n 1)"
        if [ -n "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done

    err "No OTGW-firmware-*-merged-full.bin found."
    err "        Expected in: $SCRIPT_DIR"
    err "                 or: $SCRIPT_DIR/build"
    err "        Use --bin to specify a path."
    exit 1
}

BIN_FILE="$(find_bin)"
BIN_NAME="$(basename "$BIN_FILE")"
ok "Firmware: $BIN_NAME"

# ---- Step 4: derive board from filename (or user override) -----------------
if [ -z "$ARG_BOARD" ]; then
    case "$BIN_NAME" in
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
list_ports() {
    case "$(uname -s)" in
        Linux)
            ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
            ;;
        Darwin)
            # Prefer /dev/cu.* (callout) over /dev/tty.*
            ls -1 /dev/cu.usbserial-* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART /dev/cu.wchusbserial* 2>/dev/null
            ;;
    esac
}

if [ -z "$ARG_PORT" ]; then
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
fi
ok "Port:     $ARG_PORT"

# ---- Step 6: confirm before destructive flash ------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
echo "   Firmware: $BIN_NAME"
echo "   Board:    $BOARD_NAME"
echo "   Port:     $ARG_PORT  @ $ARG_BAUD baud"
if [ "$ARG_NO_ERASE" = "1" ]; then
    echo "   Flash:    write_flash only (NOT erasing first)"
else
    echo "   Flash:    erase_flash + write_flash"
    echo "             All settings, WiFi credentials and stored data WILL BE WIPED."
fi
echo "------------------------------------------------------------"
echo

if [ "$ARG_YES" = "0" ]; then
    printf "Type YES to continue: "
    read -r CONFIRM
    if [ "$CONFIRM" != "YES" ]; then
        info "Aborted by user."
        exit 0
    fi
fi

# ---- Step 7: run esptool ---------------------------------------------------
if [ "$ARG_NO_ERASE" = "0" ]; then
    step "Running esptool erase_flash..."
    "$ESPTOOL_BIN" --chip "$ESPTOOL_CHIP" --port "$ARG_PORT" --baud "$ARG_BAUD" erase_flash
fi

step "Running esptool write_flash 0x0 $BIN_NAME..."
"$ESPTOOL_BIN" --chip "$ESPTOOL_CHIP" --port "$ARG_PORT" --baud "$ARG_BAUD" write_flash 0x0 "$BIN_FILE"

echo
echo "============================================================"
echo " Flash complete. Reset the OTGW or unplug/replug USB."
echo " Then connect to WiFi AP \"OTGW-AP\" to configure credentials,"
echo " or browse to http://otgw.local once on your network."
echo "============================================================"
