#!/usr/bin/env bash
# =============================================================================
#  flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware (Linux/macOS)
# -----------------------------------------------------------------------------
#  Distributed alongside merged binary releases. Downloads Espressif's
#  standalone esptool binary on first run (no Python required).
#
#  Default behaviour: factory reset. The script erases flash and writes the
#  merged-full image containing firmware and filesystem.
#
#  Usage:
#    ./flash_otgw.sh
#    ./flash_otgw.sh --port /dev/ttyACM0
#    ./flash_otgw.sh --bin <merged-full.bin>
#    ./flash_otgw.sh --board esp32       (Nodoshop OTGW32 / ESP32-S3)
#    ./flash_otgw.sh --baud N
#    ./flash_otgw.sh --help
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
ARG_BOARD=""
ARG_BAUD=""
ARG_MIGRATE=0
ARG_HOST=""
ARG_PASSWORD=""
ARG_NOBACKUP=0
# Migration images: slice 1 at 0x0, slice 2 right after NVS (0x9000 + 0x5000).
MIGRATE_REST_OFFSET="0xE000"

# ---- Help ------------------------------------------------------------------
show_help() {
    cat <<EOF
flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware

Usage:
  ./flash_otgw.sh [options]

Default:
  Factory reset: erase flash, then write firmware and filesystem from the
  merged-full image. WiFi credentials and settings are removed.

Targeting:
  --port <dev>         Serial port (e.g. /dev/cu.usbmodem-XXXX). The script
                       falls back to the USB VID/PID filter (303A:1001) when
                       no --port is given.
  --bin <file>         Firmware path. Use a merged-full image.
  --board esp32        Force board type (Nodoshop OTGW32 = ESP32-S3).
  --baud N             Override baud rate (default: 921600).

One-time layout migration (ADR-191, TASK-1223.8):
  --migrate            Move a unit to the new partition layout. Saves
                       settings.ini over HTTP, writes the *-migrate-0x0.bin and
                       *-migrate-rest.bin images around NVS (WiFi kept), waits
                       for the unit, restores settings.ini and checks it.
  --host <ip|name>     The unit's address (needed unless --no-backup).
  --password <pw>      Admin password, when the unit has one set.
  --no-backup          Migrate without saving settings (they return to defaults).
  --bin <x-migrate-0x0.bin>  With --migrate: pick the board's migration image
                       when several are present (they are board-specific).

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
        --migrate)  ARG_MIGRATE=1;  shift ;;
        --host)     ARG_HOST="$2";  shift 2 ;;
        --password) ARG_PASSWORD="$2"; shift 2 ;;
        --no-backup) ARG_NOBACKUP=1; shift ;;
        --help|-h)  show_help; exit 0 ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit 2 ;;
    esac
done

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
    exec sudo -E bash "$0" "${ORIGINAL_ARGS[@]}"
}
maybe_sudo_relaunch

echo
echo "============================================================"
echo " OTGW Flash Tool ($(uname -s))"
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

# ---- Migration (TASK-1223.8): its own path, then exit -----------------------
migrate_flow() {
    local first="" rest dir f
    if [ "$ARG_NOBACKUP" != "1" ] && [ -z "$ARG_HOST" ]; then
        err "--migrate needs --host <device ip or name>, or --no-backup."
        exit 2
    fi
    # --bin <...-migrate-0x0.bin> picks one explicitly. Without it exactly one
    # candidate must exist: the images are board-specific.
    local cands=()
    if [ -n "$ARG_BIN" ]; then
        [ -f "$ARG_BIN" ] || { err "--bin file not found: $ARG_BIN"; exit 1; }
        cands=("$ARG_BIN")
    else
        for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
            for f in "$dir"/OTGW-firmware-*-migrate-0x0.bin; do
                [ -f "$f" ] && cands+=("$f")
            done
            [ ${#cands[@]} -gt 0 ] && break
        done
    fi
    [ ${#cands[@]} -gt 0 ] || { err "No *-migrate-0x0.bin found next to this script or in build/."; exit 1; }
    if [ ${#cands[@]} -gt 1 ]; then
        err "Several migration images found; they are board-specific. Pick one with --bin:"
        for f in "${cands[@]}"; do echo "          $(basename "$f")" >&2; done
        exit 1
    fi
    first="${cands[0]}"
    rest="${first%-migrate-0x0.bin}-migrate-rest.bin"
    [ -f "$rest" ] || { err "Missing the matching image: $rest"; exit 1; }

    local port_args baud auth=() backup=""
    baud="${ARG_BAUD:-921600}"
    if [ -n "$ARG_PORT" ]; then port_args="--port $ARG_PORT"
    else port_args="--port-filter vid=0x303A --port-filter pid=0x1001"; fi
    [ -n "$ARG_PASSWORD" ] && auth=(-u "admin:$ARG_PASSWORD")

    if [ "$ARG_NOBACKUP" != "1" ]; then
        backup="$SCRIPT_DIR/backup/${ARG_HOST}-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$backup"
        step "Saving settings from http://$ARG_HOST to $backup"
        curl -sf --max-time 20 ${auth[@]+"${auth[@]}"} -o "$backup/settings.ini" "http://$ARG_HOST/settings.ini" \
            || { err "Could not download settings.ini. Migration stopped, nothing written."; exit 1; }
        [ -s "$backup/settings.ini" ] || { err "settings.ini downloaded empty. Migration stopped, nothing written."; exit 1; }
        curl -sf --max-time 20 ${auth[@]+"${auth[@]}"} -o "$backup/dallas_labels.ini" "http://$ARG_HOST/dallas_labels.ini" \
            || rm -f "$backup/dallas_labels.ini"
        ok "Saved settings.ini ($(wc -c < "$backup/settings.ini") bytes)"
    fi

    step "Writing $(basename "$first") @0x0 and $(basename "$rest") @$MIGRATE_REST_OFFSET (NVS kept)"
    # shellcheck disable=SC2086
    "$ESPTOOL_BIN" --chip esp32s3 $port_args --baud "$baud" \
        --before default_reset --after hard_reset \
        write_flash -z 0x0 "$first" "$MIGRATE_REST_OFFSET" "$rest"

    [ "$ARG_NOBACKUP" = "1" ] && { ok "Migration done. Settings are at defaults."; exit 0; }

    wait_for_host() {
        local i
        info "Waiting for http://$ARG_HOST (up to 3 minutes)..."
        for i in $(seq 1 60); do
            curl -sf --max-time 3 -o /dev/null ${auth[@]+"${auth[@]}"} "http://$ARG_HOST/api/v2/device/info" && { ok "Unit is back"; return 0; }
            sleep 3
        done
        err "The unit did not come back at http://$ARG_HOST/. Saved settings: $backup"
        exit 1
    }
    wait_for_host
    step "Restoring settings.ini"
    curl -sf --max-time 30 ${auth[@]+"${auth[@]}"} -o /dev/null -F "upload=@$backup/settings.ini;filename=settings.ini" \
        "http://$ARG_HOST/upload?path=/" \
        || { err "Upload of settings.ini failed. Restore it by hand from $backup."; exit 1; }
    if [ -f "$backup/dallas_labels.ini" ]; then
        curl -sf --max-time 30 ${auth[@]+"${auth[@]}"} -o /dev/null -F "upload=@$backup/dallas_labels.ini;filename=dallas_labels.ini" \
            "http://$ARG_HOST/upload?path=/" || warn "Upload of dallas_labels.ini failed."
    fi
    curl -s --max-time 10 ${auth[@]+"${auth[@]}"} -o /dev/null "http://$ARG_HOST/ReBoot" || true
    sleep 10
    wait_for_host
    curl -sf --max-time 20 ${auth[@]+"${auth[@]}"} -o "$backup/settings-after.ini" "http://$ARG_HOST/settings.ini" || true
    if cmp -s "$backup/settings.ini" "$backup/settings-after.ini"; then
        ok "Settings restored and verified (byte-identical)"
    else
        err "settings.ini on the unit differs from the saved copy. Restore it by hand from $backup."
        exit 1
    fi
    echo
    echo "============================================================"
    echo " Migration done. Firmware updates now work over WiFi through"
    echo " the /update page. Saved settings: $backup"
    echo "============================================================"
    exit 0
}
[ "$ARG_MIGRATE" = "1" ] && migrate_flow

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

    # Collect all merged-full candidates from the first dir that has any. A
    # release zip carries exactly one bin -> single match -> auto-selected. When
    # several coexist (e.g. both ESP32-S3 targets, esp32 and esp32-combo, in
    # build/) we list them and let the user choose instead of silently taking
    # the first.
    local cands=() dir f
    for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
        for f in "$dir"/OTGW-firmware-*-merged-full.bin; do
            [ -f "$f" ] && cands+=("$f")
        done
        [ ${#cands[@]} -gt 0 ] && break
    done

    if [ ${#cands[@]} -eq 0 ]; then
        err "No merged-full bin found."
        err "        Expected in: $SCRIPT_DIR"
        err "                 or: $SCRIPT_DIR/build"
        err "        Use --bin to specify a path."
        exit 1
    fi

    if [ ${#cands[@]} -eq 1 ]; then
        echo "${cands[0]}"
        return 0
    fi

    # Multiple candidates. Prompt on stderr and read from the tty so the chosen
    # path stays the only thing on stdout (this function's output is captured).
    {
        echo ""
        echo "[INFO] Multiple firmware images found:"
        local i=1
        for f in "${cands[@]}"; do
            echo "  [$i] $(basename "$f")"
            i=$((i + 1))
        done
        echo ""
        printf "Select firmware number [1-%d]: " "${#cands[@]}"
    } >&2
    local choice=""
    read -r choice < /dev/tty
    case "$choice" in
        ''|*[!0-9]*) err "Invalid firmware selection."; exit 1 ;;
    esac
    if [ "$choice" -lt 1 ] || [ "$choice" -gt ${#cands[@]} ]; then
        err "Invalid firmware selection."
        exit 1
    fi
    echo "${cands[$((choice - 1))]}"
}

BIN_FILE="$(find_bin)"
BIN_NAME="$(basename "$BIN_FILE")"

# ---- Step 4: derive board from filename (or user override) -----------------
# All 2.0.0 targets are ESP32-S3 (esp32-otgw32, esp32-classic, esp32-combo),
# so any merged-full image maps to the esp32 board.
if [ -z "$ARG_BOARD" ]; then
    case "$BIN_NAME" in
        *-esp32-*) ARG_BOARD="esp32" ;;
    esac
fi

case "$ARG_BOARD" in
    esp32)
        ESPTOOL_CHIP="esp32s3"
        BOARD_NAME="Nodoshop OTGW32 (ESP32-S3)"
        [ -z "$ARG_BAUD" ] && ARG_BAUD="921600"
        ;;
    "")
        err "Could not detect board from filename. Use --board esp32"
        exit 1
        ;;
    *)
        err "Unknown board: $ARG_BOARD (expected esp32)"
        exit 1
        ;;
esac

ok "Board:    $BOARD_NAME"
ok "Baud:     $ARG_BAUD"

# ---- Step 5: locate serial port --------------------------------------------
# ESP32-S3 has a fixed USB VID/PID (303A:1001 = built-in USB-Serial JTAG), so
# esptool can find it itself via --port-filter when no explicit --port is given.
PORT_ARGS=""
if [ -n "$ARG_PORT" ]; then
    PORT_ARGS="--port $ARG_PORT"
    ok "Port:     $ARG_PORT"
else
    PORT_ARGS="--port-filter vid=0x303A --port-filter pid=0x1001"
    ok "Port:     auto-detect via USB VID/PID 303A:1001"
fi

ok "Firmware: $BIN_NAME"

# ---- Step 6: show selected flash action ------------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
echo "   Firmware: $BIN_NAME"
echo "   Board:    $BOARD_NAME"
echo "   Baud:     $ARG_BAUD"
echo "   Mode:     factory reset"
echo "   Effect:   Erases flash, then writes firmware and filesystem."
echo "             WiFi credentials and settings will be removed."
echo "------------------------------------------------------------"
echo

# ---- Step 7: run esptool ---------------------------------------------------
step "Running esptool write_flash..."
"$ESPTOOL_BIN" --chip "$ESPTOOL_CHIP" $PORT_ARGS --baud "$ARG_BAUD" \
    --before default_reset --after hard_reset \
    write_flash -z -e 0x0 "$BIN_FILE"

echo
echo "============================================================"
echo " Flash complete."
echo " After reset: connect to WiFi AP \"OTGW-AP\" to configure."
echo "============================================================"
