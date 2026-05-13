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
#  If the release binaries are not present in the script directory, they are
#  auto-downloaded from the latest GitHub release and integrity-checked
#  against the release's SHA256SUMS asset.
#
#  Serial port is auto-detected by USB VID/PID (CP210x / CH340 / FTDI).
#
#  Usage:
#    ./flash_otgw.sh                         # auto-detect everything
#    ./flash_otgw.sh --port /dev/ttyUSB0     # explicit port
#    ./flash_otgw.sh --list-ports            # show detected ports and exit
#    ./flash_otgw.sh --yes                   # skip confirmation prompt
#    ./flash_otgw.sh --help
#
#  Supported hardware: Nodoshop OTGW with Wemos D1 mini / NodeMCU (ESP8266)
# =============================================================================

set -e
set -u
set -o pipefail 2>/dev/null || true

ORIGINAL_ARGS=("$@")

# ---- Pinned versions / hashes -----------------------------------------------
# Keep ESPTOOL_VERSION in sync with flash_otgw.bat
# (enforced by .github/workflows/flash-scripts-lint.yml).
ESPTOOL_VERSION="v4.8.1"

# Pinned esptool SHA256 (https://github.com/espressif/esptool/releases/tag/v4.8.1)
ESPTOOL_SHA256_LINUX_AMD64="84c18a8b46224f53ddf69775945600558eaceb35f61892bfeea65a248e6bd874"
ESPTOOL_SHA256_LINUX_ARM64="dbbe06ccf2fa4b33e5e6f466db740235b08891d5b1d57ef1b0091915cec60562"
ESPTOOL_SHA256_LINUX_ARM32="5ea486d974a7cbc0dfcc329ab8674e77b50d06412da1cea9fa1865f68adb18ee"
ESPTOOL_SHA256_MACOS="d25735b1b8939003dae6d1f193668d6c9e565e0f581606f44d7396b0ca623906"

GITHUB_REPO="rvdbreemen/OTGW-firmware"
GITHUB_API_LATEST="https://api.github.com/repos/${GITHUB_REPO}/releases/latest"
GITHUB_LATEST_REDIRECT="https://github.com/${GITHUB_REPO}/releases/latest/download"

# USB-serial VID/PID allowlist (lower-case hex, no separator)
USB_VID_PID_LIST="10c4:ea60 1a86:7523 0403:6001"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLS_DIR="${SCRIPT_DIR}/tools/esptool"
ESPTOOL_BIN="${TOOLS_DIR}/esptool"

# Exit codes
EXIT_OK=0
EXIT_GENERIC=1
EXIT_BAD_ARGS=2
EXIT_SHA_MISMATCH=3

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
ARG_YES=0
ARG_LIST_PORTS=0
ARG_NO_SUDO=0

# ---- Help -------------------------------------------------------------------
show_help() {
    cat <<EOF
NAME
  flash_otgw.sh - Zero-install flash tool for OTGW-firmware (ESP8266)

SYNOPSIS
  ./flash_otgw.sh [options]

DESCRIPTION
  Flashes the firmware (0x0) and filesystem (0x200000) onto a Nodoshop OTGW
  WiFi module. Looks for the release binaries next to this script:
      OTGW-firmware-*.ino.bin       firmware
      OTGW-firmware*.littlefs.bin   filesystem
  If either is missing, downloads the latest release from GitHub and
  verifies SHA256 against the release's SHA256SUMS asset.

  The serial port is auto-detected by USB VID/PID (CP210x, CH340, FTDI).
  If multiple matching adapters are found, the first is used; pass --port
  to override.

OPTIONS
  --port <dev>    Serial port (e.g. /dev/ttyUSB0, /dev/cu.usbserial-XXXX).
  --baud N        Override baud rate (default: ${ARG_BAUD}).
  --yes, -y       Skip the "Continue? [y/N]" confirmation prompt.
  --list-ports    List detected serial ports (with VID/PID) and exit.
  --no-sudo       Disable silent sudo escalation on Linux. With --no-sudo
                  and a non-writable port, prints instructions for adding
                  yourself to the dialout group and exits non-zero.
  --help, -h      Show this help.

FIRST-RUN BEHAVIOUR
  Downloads esptool ${ESPTOOL_VERSION} to ./tools/esptool/ (SHA256-verified).
  On Linux, silently re-executes under sudo when the serial device is not
  writable (override with --no-sudo).

AFTER FLASHING
  The device opens a WiFi AP named "OTGW-<MAC-address>".
  Connect and browse to http://192.168.4.1 to configure WiFi.
EOF
}

# ---- Parse arguments --------------------------------------------------------
while [ $# -gt 0 ]; do
    case "$1" in
        --port)         ARG_PORT="$2"; shift 2 ;;
        --baud)         ARG_BAUD="$2"; shift 2 ;;
        --yes|-y)       ARG_YES=1; shift ;;
        --list-ports)   ARG_LIST_PORTS=1; shift ;;
        --no-sudo)      ARG_NO_SUDO=1; shift ;;
        --help|-h)      show_help; exit "$EXIT_OK" ;;
        *) err "Unknown argument: $1"; echo "Run './flash_otgw.sh --help' for usage."; exit "$EXIT_BAD_ARGS" ;;
    esac
done

# ---- Generic helpers --------------------------------------------------------
sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        err "sha256sum or shasum is required for integrity verification."
        return 1
    fi
}

verify_sha256() {
    # $1 = path, $2 = expected hex
    local actual
    actual="$(sha256_of "$1")" || return 1
    if [ "$actual" != "$2" ]; then
        err "SHA256 mismatch for $(basename "$1")"
        err "  expected: $2"
        err "  actual:   $actual"
        return 1
    fi
}

http_get() {
    # $1 = URL, $2 = output path. Echoes HTTP status (or "000" on transport error).
    local url="$1" out="$2" code
    if command -v curl >/dev/null 2>&1; then
        code="$(curl -fsSL --retry 2 -A "OTGW-Flash-Tool" -w "%{http_code}" -o "$out" "$url" 2>/dev/null || echo "000")"
    elif command -v wget >/dev/null 2>&1; then
        if wget -q --user-agent="OTGW-Flash-Tool" -O "$out" "$url"; then
            code="200"
        else
            code="000"
        fi
    else
        err "Neither curl nor wget is available. Install one and re-run."
        return 1
    fi
    echo "$code"
}

# ---- Linux: silent sudo escalation (default) --------------------------------
maybe_sudo_relaunch() {
    [ "$(uname -s)" = "Linux" ] || return 0
    [ "${EUID:-$(id -u)}" -ne 0 ] || return 0   # already root

    local need_sudo=0 d
    for d in /dev/ttyUSB0 /dev/ttyACM0 /dev/ttyUSB1 /dev/ttyACM1; do
        [ -e "$d" ] || continue
        [ -w "$d" ] || need_sudo=1
    done
    [ "$need_sudo" = "1" ] || return 0

    # Already in dialout/uucp? Bad group cache, not a permission issue.
    local grp
    for grp in dialout uucp; do
        if id -nG "$USER" 2>/dev/null | grep -qw "$grp"; then
            warn "User $USER is in '$grp' but cannot write to the serial device."
            warn "Try logging out and back in to refresh group membership."
            return 0
        fi
    done

    if [ "$ARG_NO_SUDO" = "1" ]; then
        err "Serial device is not writable and --no-sudo was passed."
        err "To grant permanent access without sudo, add yourself to the"
        err "serial group and log out / back in:"
        err "    sudo usermod -aG dialout \$USER     # Debian/Ubuntu"
        err "    sudo usermod -aG uucp \$USER        # Arch/Fedora"
        exit "$EXIT_GENERIC"
    fi

    info "Serial device not writable — re-running with sudo..."
    exec sudo -E bash "$0" "${ORIGINAL_ARGS[@]}"
}

# ---- Platform detection -----------------------------------------------------
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

expected_esptool_sha256() {
    case "$1" in
        linux-amd64) echo "$ESPTOOL_SHA256_LINUX_AMD64" ;;
        linux-arm64) echo "$ESPTOOL_SHA256_LINUX_ARM64" ;;
        linux-arm32) echo "$ESPTOOL_SHA256_LINUX_ARM32" ;;
        macos)       echo "$ESPTOOL_SHA256_MACOS" ;;
        *) return 1 ;;
    esac
}

# ---- Ensure esptool is present (with SHA256 verification) -------------------
ensure_esptool() {
    if [ -x "$ESPTOOL_BIN" ]; then
        ok "esptool: $ESPTOOL_BIN"
        return 0
    fi

    local platform_tag asset_name url zip_path expected_sha code
    platform_tag="$(detect_platform)" || exit "$EXIT_GENERIC"
    asset_name="esptool-${ESPTOOL_VERSION}-${platform_tag}.zip"
    url="https://github.com/espressif/esptool/releases/download/${ESPTOOL_VERSION}/${asset_name}"
    expected_sha="$(expected_esptool_sha256 "$platform_tag")" \
        || { err "No pinned SHA256 for platform $platform_tag."; exit "$EXIT_GENERIC"; }

    info "Downloading esptool ${ESPTOOL_VERSION} (${platform_tag})..."
    mkdir -p "$TOOLS_DIR"
    zip_path="${TOOLS_DIR}/${asset_name}"

    code="$(http_get "$url" "$zip_path")"
    if [ "$code" != "200" ] && [ "$code" != "302" ]; then
        err "Download failed (HTTP $code): $url"
        exit "$EXIT_GENERIC"
    fi

    info "Verifying esptool SHA256..."
    verify_sha256 "$zip_path" "$expected_sha" \
        || { rm -f "$zip_path"; exit "$EXIT_SHA_MISMATCH"; }
    ok "esptool SHA256 verified"

    command -v unzip >/dev/null 2>&1 \
        || { err "'unzip' is required. Install it and re-run."; exit "$EXIT_GENERIC"; }
    info "Extracting esptool..."
    unzip -q -o "$zip_path" -d "$TOOLS_DIR"

    local found
    found="$(find "$TOOLS_DIR" -maxdepth 4 -type f -name esptool 2>/dev/null | head -n 1)"
    [ -n "$found" ] || { err "Extracted archive did not contain an esptool binary."; exit "$EXIT_GENERIC"; }

    cp "$found" "$ESPTOOL_BIN"
    chmod +x "$ESPTOOL_BIN"
    rm -f "$zip_path"
    ok "esptool installed: $ESPTOOL_BIN"
}

# ---- Version-aware file selection -------------------------------------------
find_highest_version() {
    # $1 = directory, $2 = glob pattern. Outputs the highest-version match (sort -V).
    # shellcheck disable=SC2086
    ls -1 $1/$2 2>/dev/null | sort -V -r | head -n 1
}

extract_firmware_version() {
    # OTGW-firmware-1.10.0.ino.bin -> 1.10.0
    basename "$1" | sed -E 's/^OTGW-firmware-?(.+)\.ino\.bin$/\1/'
}

# ---- GitHub release download ------------------------------------------------
fetch_release_json() {
    # Writes JSON to $1, echoes HTTP status.
    http_get "$GITHUB_API_LATEST" "$1"
}

parse_asset_urls_from_json() {
    # $1 = release.json path. Echoes <name>\t<url> for each asset.
    # Avoids jq dependency: greps "name" / "browser_download_url" pairs.
    sed -n 's/.*"name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/N\t\1/p;
            s/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/U\t\1/p' "$1" \
    | awk 'BEGIN{n=""} /^N\t/{n=substr($0,3); next} /^U\t/{ if(n!="") print n "\t" substr($0,3); n="" }'
}

download_release_binaries() {
    info "Fetching latest release info from GitHub..."
    local tmp_json="${SCRIPT_DIR}/.release.json" code
    code="$(fetch_release_json "$tmp_json")"

    if [ "$code" = "200" ]; then
        # API path: parse JSON for asset URLs
        local pairs
        pairs="$(parse_asset_urls_from_json "$tmp_json")"
        rm -f "$tmp_json"
        if [ -z "$pairs" ]; then
            err "GitHub API returned no parseable assets."
            return 1
        fi
        local name url got=0
        while IFS=$'\t' read -r name url; do
            case "$name" in
                *.ino.bin|*.littlefs.bin|SHA256SUMS)
                    info "Downloading ${name}..."
                    local dl_code
                    dl_code="$(http_get "$url" "${SCRIPT_DIR}/${name}")"
                    [ "$dl_code" = "200" ] || { err "Download failed for $name (HTTP $dl_code)"; return 1; }
                    ok "${name}"
                    got=$((got + 1))
                    ;;
            esac
        done <<< "$pairs"
        [ "$got" -gt 0 ] || { err "No firmware/filesystem assets found in release."; return 1; }
    else
        # Rate-limited (403) or 5xx — fall back to the static redirect.
        warn "GitHub API returned HTTP $code — falling back to releases/latest/download/"
        rm -f "$tmp_json"
        # We don't know the version-stamped filenames without the API, but
        # SHA256SUMS at releases/latest/download/SHA256SUMS always resolves.
        local sums="${SCRIPT_DIR}/SHA256SUMS"
        local dl_code
        dl_code="$(http_get "${GITHUB_LATEST_REDIRECT}/SHA256SUMS" "$sums")"
        if [ "$dl_code" != "200" ]; then
            err "Fallback failed: could not fetch SHA256SUMS (HTTP $dl_code)."
            err "This release may not publish a SHA256SUMS asset yet."
            return 1
        fi
        # Parse filenames from SHA256SUMS, download each via the redirect.
        local fn
        while read -r _hash fn; do
            [ -n "$fn" ] || continue
            case "$fn" in
                *.ino.bin|*.littlefs.bin)
                    info "Downloading ${fn}..."
                    dl_code="$(http_get "${GITHUB_LATEST_REDIRECT}/${fn}" "${SCRIPT_DIR}/${fn}")"
                    [ "$dl_code" = "200" ] || { err "Download failed for $fn (HTTP $dl_code)"; return 1; }
                    ok "${fn}"
                    ;;
            esac
        done < "$sums"
    fi
    return 0
}

verify_against_sums() {
    # $1 = binary path, $2 = SHA256SUMS path
    local file="$1" sums="$2" fn expected actual
    fn="$(basename "$file")"
    expected="$(awk -v f="$fn" 'BEGIN{IGNORECASE=0} { if ($2 == f || $2 == "*"f) { print $1; exit } }' "$sums")"
    if [ -z "$expected" ]; then
        err "No SHA256 entry for $fn in SHA256SUMS."
        return 1
    fi
    actual="$(sha256_of "$file")" || return 1
    if [ "$expected" != "$actual" ]; then
        err "SHA256 mismatch for $fn"
        err "  expected: $expected"
        err "  actual:   $actual"
        return 1
    fi
    ok "SHA256 verified: $fn"
}

# ---- Serial port detection (VID/PID-aware) ----------------------------------
linux_port_info() {
    # $1 = device path (/dev/ttyUSB0 etc). Echoes: <vid:pid>\t<description>
    # Returns 1 if no USB-serial sysfs entry.
    local dev="$1" base sys vid pid product manufacturer
    base="$(basename "$dev")"
    sys="/sys/class/tty/${base}"
    [ -e "$sys/device" ] || return 1
    local devpath
    devpath="$(realpath "$sys/device" 2>/dev/null)" || return 1
    # Walk up to find idVendor/idProduct
    while [ -n "$devpath" ] && [ "$devpath" != "/" ]; do
        if [ -f "$devpath/idVendor" ] && [ -f "$devpath/idProduct" ]; then
            vid="$(cat "$devpath/idVendor" 2>/dev/null)"
            pid="$(cat "$devpath/idProduct" 2>/dev/null)"
            product="$(cat "$devpath/product" 2>/dev/null || true)"
            manufacturer="$(cat "$devpath/manufacturer" 2>/dev/null || true)"
            printf "%s:%s\t%s %s\n" "$vid" "$pid" "$manufacturer" "$product"
            return 0
        fi
        devpath="$(dirname "$devpath")"
    done
    return 1
}

macos_port_info() {
    # $1 = device path (/dev/cu.usbserial-XXXX). Echoes: <vid:pid>\t<description>
    # Uses ioreg to map BSD name to USB VID/PID.
    local dev="$1" bsd
    bsd="$(basename "$dev")"
    # ioreg prints a tree; grep the IOCalloutDevice line, then walk up for VID/PID.
    # Best-effort: parse IORegistry XML.
    local out
    out="$(ioreg -r -c IOUSBHostDevice -l 2>/dev/null || ioreg -r -c IOUSBDevice -l 2>/dev/null || true)"
    [ -n "$out" ] || return 1
    # awk: track current device block, capture vid/pid/product/calloutdev.
    echo "$out" | awk -v bsd="$bsd" '
        /^[[:space:]]*\+-o/ { vid=""; pid=""; product=""; cd="" }
        /"idVendor"/ { match($0, /= ([0-9]+)/, a); if (a[1]!="") vid=sprintf("%04x", a[1]) }
        /"idProduct"/ { match($0, /= ([0-9]+)/, a); if (a[1]!="") pid=sprintf("%04x", a[1]) }
        /"USB Product Name"/ { match($0, /= "([^"]*)"/, a); product=a[1] }
        /"IOCalloutDevice"/ {
            match($0, /= "([^"]*)"/, a)
            if (a[1] ~ bsd"$") { print vid ":" pid "\t" product; exit }
        }
    '
}

port_vid_pid_match() {
    # $1 = "vid:pid". Returns 0 if in allowlist.
    local needle="$1" entry
    for entry in $USB_VID_PID_LIST; do
        [ "$needle" = "$entry" ] && return 0
    done
    return 1
}

candidate_ports() {
    case "$(uname -s)" in
        Linux)  ls -1 /dev/ttyUSB* /dev/ttyACM* 2>/dev/null ;;
        Darwin) ls -1 /dev/cu.usbserial-* /dev/cu.usbmodem* /dev/cu.SLAB_USBtoUART \
                       /dev/cu.wchusbserial* 2>/dev/null ;;
    esac
}

list_serial_ports() {
    local kernel p info vidpid desc
    kernel="$(uname -s)"
    local found=0
    while IFS= read -r p; do
        [ -n "$p" ] || continue
        case "$kernel" in
            Linux)  info="$(linux_port_info "$p" 2>/dev/null || true)" ;;
            Darwin) info="$(macos_port_info "$p" 2>/dev/null || true)" ;;
            *)      info="" ;;
        esac
        if [ -n "$info" ]; then
            vidpid="$(printf '%s' "$info" | cut -f1)"
            desc="$(printf '%s' "$info" | cut -f2-)"
        else
            vidpid="?"
            desc=""
        fi
        printf "  %-30s %-12s %s\n" "$p" "$vidpid" "$desc"
        found=$((found + 1))
    done < <(candidate_ports | sort)
    [ "$found" -gt 0 ] || info "No serial ports detected."
}

detect_port() {
    local kernel="$(uname -s)"
    local matched=""
    local fallback=""
    local p info vidpid

    while IFS= read -r p; do
        [ -n "$p" ] || continue
        [ -z "$fallback" ] && fallback="$p"
        case "$kernel" in
            Linux)  info="$(linux_port_info "$p" 2>/dev/null || true)" ;;
            Darwin) info="$(macos_port_info "$p" 2>/dev/null || true)" ;;
            *)      info="" ;;
        esac
        if [ -n "$info" ]; then
            vidpid="$(printf '%s' "$info" | cut -f1)"
            if port_vid_pid_match "$vidpid"; then
                matched="$p"
                break
            fi
        fi
    done < <(candidate_ports | sort)

    if [ -n "$matched" ]; then
        echo "$matched"
    elif [ -n "$fallback" ]; then
        warn "No USB-serial adapter with known VID/PID found — using first port: $fallback" >&2
        echo "$fallback"
    fi
}

# ---- Confirmation prompt ----------------------------------------------------
confirm_or_exit() {
    [ "$ARG_YES" = "1" ] && return 0
    if [ ! -t 0 ]; then
        err "stdin is not a terminal and --yes was not passed. Aborting."
        exit "$EXIT_GENERIC"
    fi
    local ans=""
    printf "Continue? [y/N] (auto-cancel in 30s): "
    if ! read -t 30 -r ans 2>/dev/null; then
        echo ""
        err "Timed out waiting for confirmation. Aborting."
        exit "$EXIT_GENERIC"
    fi
    case "$ans" in
        y|Y|yes|YES) return 0 ;;
        *) info "Aborted by user."; exit "$EXIT_OK" ;;
    esac
}

# =============================================================================
# Main flow
# =============================================================================

# --list-ports short-circuits before any download/escalation
if [ "$ARG_LIST_PORTS" = "1" ]; then
    echo "Detected serial ports:"
    list_serial_ports
    exit "$EXIT_OK"
fi

maybe_sudo_relaunch

echo
echo "============================================================"
echo " OTGW Flash Tool  ($(uname -s))"
echo " Target: Nodoshop OTGW WiFi module (ESP8266)"
echo " esptool: ${ESPTOOL_VERSION}"
echo "============================================================"
echo

ensure_esptool

# ---- Locate firmware and filesystem binaries (auto-download if missing) -----
FW_FILE=""
FS_FILE=""

for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
    [ -d "$dir" ] || continue
    [ -z "$FW_FILE" ] && FW_FILE="$(find_highest_version "$dir" "OTGW-firmware-*.ino.bin")"
    [ -z "$FS_FILE" ] && FS_FILE="$(find_highest_version "$dir" "OTGW-firmware*.littlefs.bin")"
done

VERIFIED_FROM_RELEASE=0
if [ -z "$FW_FILE" ] || [ -z "$FS_FILE" ]; then
    info "Release binaries not found locally — attempting auto-download."
    if ! download_release_binaries; then
        err "Auto-download failed. Download the release binaries manually"
        err "from https://github.com/${GITHUB_REPO}/releases and place them"
        err "in the same directory as this script."
        exit "$EXIT_GENERIC"
    fi
    # Re-scan
    FW_FILE="$(find_highest_version "$SCRIPT_DIR" "OTGW-firmware-*.ino.bin")"
    FS_FILE="$(find_highest_version "$SCRIPT_DIR" "OTGW-firmware*.littlefs.bin")"
    [ -n "$FW_FILE" ] || { err "Firmware binary not found after download."; exit "$EXIT_GENERIC"; }
    [ -n "$FS_FILE" ] || { err "Filesystem binary not found after download."; exit "$EXIT_GENERIC"; }
    VERIFIED_FROM_RELEASE=1
fi

# Verify SHA256 for auto-downloaded files
if [ "$VERIFIED_FROM_RELEASE" = "1" ]; then
    SUMS_FILE="${SCRIPT_DIR}/SHA256SUMS"
    if [ ! -f "$SUMS_FILE" ]; then
        # API path didn't include SHA256SUMS — try fetching it directly.
        info "Fetching SHA256SUMS from release..."
        code="$(http_get "${GITHUB_LATEST_REDIRECT}/SHA256SUMS" "$SUMS_FILE")"
        if [ "$code" != "200" ]; then
            err "Could not fetch SHA256SUMS (HTTP $code)."
            err "This release may not publish a SHA256SUMS asset yet."
            exit "$EXIT_SHA_MISMATCH"
        fi
    fi
    info "Verifying release binaries against SHA256SUMS..."
    verify_against_sums "$FW_FILE" "$SUMS_FILE" || exit "$EXIT_SHA_MISMATCH"
    verify_against_sums "$FS_FILE" "$SUMS_FILE" || exit "$EXIT_SHA_MISMATCH"
else
    info "Using local binaries — SHA256 verification skipped."
fi

FW_VER="$(extract_firmware_version "$FW_FILE")"
ok "Firmware:   $(basename "$FW_FILE")"
ok "Filesystem: $(basename "$FS_FILE")"
ok "Version:    $FW_VER"
ok "Baud:       $ARG_BAUD"

# ---- Auto-detect serial port -----------------------------------------------
if [ -n "$ARG_PORT" ]; then
    ok "Port:       $ARG_PORT (specified)"
else
    ARG_PORT="$(detect_port)"
    if [ -z "$ARG_PORT" ]; then
        err "No serial ports found. Connect your OTGW via USB and try again."
        err "        Linux:  expect /dev/ttyUSB* or /dev/ttyACM*"
        err "        macOS:  expect /dev/cu.usbserial-* or /dev/cu.usbmodem*"
        err "Run with --list-ports to see what was detected."
        exit "$EXIT_GENERIC"
    fi
    ok "Port:       $ARG_PORT (auto-detected)"
fi

# ---- Summary ----------------------------------------------------------------
echo
echo "------------------------------------------------------------"
echo " Ready to flash:"
printf "   Firmware:   %s\n" "$(basename "$FW_FILE")"
printf "   Filesystem: %s\n" "$(basename "$FS_FILE")"
echo "   Version:    $FW_VER"
echo "   Port:       $ARG_PORT"
echo "   Baud:       $ARG_BAUD"
echo ""
echo "   Erases flash, then writes firmware at 0x0"
echo "   and filesystem at 0x200000."
echo "------------------------------------------------------------"
echo

confirm_or_exit

# ---- Flash ------------------------------------------------------------------
step "Flashing firmware and filesystem..."

if ! "$ESPTOOL_BIN" --chip esp8266 --port "$ARG_PORT" --baud "$ARG_BAUD" \
    --before default_reset --after hard_reset \
    write_flash -z --erase-all \
    0x0       "$FW_FILE" \
    0x200000  "$FS_FILE"; then
    echo
    err "Flash failed."
    err "  Troubleshooting:"
    err "    - Try a different USB cable (data cable, not charge-only)"
    err "    - Install CP210x or CH340 USB-serial drivers if missing"
    err "    - Try a lower baud rate: --baud 115200"
    err "    - Specify the port explicitly: --port $ARG_PORT"
    exit "$EXIT_GENERIC"
fi

echo
echo "============================================================"
echo " Flash complete."
echo " Connect to WiFi AP 'OTGW-<MAC-address>' and browse to"
echo " http://192.168.4.1 to configure WiFi settings."
echo "============================================================"
