#!/usr/bin/env bash
# capture-mqtt-debug.sh - OTGW diagnostic capture launcher (Linux and macOS).
#
# The capture itself is otgw_capture.py next to this file (ADR-202). This
# launcher only finds a Python 3.9+ interpreter and runs it with every argument:
#   1. python3 or python on PATH;
#   2. otherwise a portable CPython 3.12.16 (python-build-standalone, release
#      20261026) that an earlier run unpacked into
#        Linux:  ${XDG_DATA_HOME:-$HOME/.local/share}/otgw-capture/
#        macOS:  $HOME/Library/Application Support/otgw-capture/
#   3. otherwise that runtime is downloaded once, its SHA-256 is checked against
#      the value pinned below, and it is unpacked into the folder above.
#
#   scripts/capture-mqtt-debug.sh
#   scripts/capture-mqtt-debug.sh --device 192.168.1.50 --broker 192.168.1.10 --duration 120
#   scripts/capture-mqtt-debug.sh --help

set -u

SCRIPT_DIR="$(CDPATH='' cd -- "$(dirname "$0")" && pwd)"
TOOL="$SCRIPT_DIR/otgw_capture.py"

if [[ ! -f "$TOOL" ]]; then
  echo "otgw_capture.py was not found next to this launcher: $TOOL" >&2
  echo "Unzip the whole flash zip, or run this from the scripts folder." >&2
  exit 1
fi

PY_VER="3.12.16"
PBS_TAG="20261026"
PBS_BASE="https://github.com/astral-sh/python-build-standalone/releases/download/$PBS_TAG"
PY_MIN_CHECK='import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)'

# Prints the folder the portable runtime lives in. macOS keeps it under
# Library/Application Support, Linux under XDG_DATA_HOME.
runtime_parent() {
  if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "${HOME:-/tmp}/Library/Application Support/otgw-capture"
  else
    echo "${XDG_DATA_HOME:-${HOME:-/tmp}/.local/share}/otgw-capture"
  fi
}

# Step 2: a runtime unpacked by an earlier run, used without any network access.
use_cached_python() {
  local cached
  cached="$(runtime_parent)/python-${PY_VER}/python/bin/python3"
  if [[ -x "$cached" ]] && "$cached" -c "$PY_MIN_CHECK" >/dev/null 2>&1; then
    exec "$cached" "$TOOL" "$@"
  fi
  return 1
}

BOOT_TMP=""
boot_cleanup() {
  if [[ -n "$BOOT_TMP" ]]; then
    rm -rf "$BOOT_TMP"
    BOOT_TMP=""
  fi
}

# One message, the manual alternative, no partial files left behind.
boot_fail() {
  boot_cleanup
  echo "ERROR: $1" >&2
  echo "Install Python 3.9 or newer yourself and run this again:" >&2
  echo "  macOS:         xcode-select --install   (or: brew install python)" >&2
  echo "  Debian/Ubuntu: sudo apt install python3" >&2
  echo "  Fedora:        sudo dnf install python3" >&2
  exit 1
}

# Step 3: download, verify, unpack, then move into place in one step.
download_python() {
  local os arch triple PY_SHA asset parent final url
  os="$(uname -s)"
  arch="$(uname -m)"
  echo "Python 3.9 or newer was not found."

  case "$os" in
    Linux)
      if ls /lib/ld-musl-* >/dev/null 2>&1 || ldd --version 2>&1 | grep -qi musl; then
        boot_fail "this Linux uses musl libc (for example Alpine); there is no portable Python for it."
      fi
      case "$arch" in
        x86_64|amd64)  triple="x86_64-unknown-linux-gnu" ;;
        aarch64|arm64) triple="aarch64-unknown-linux-gnu" ;;
        armv7l|armv7)  triple="armv7-unknown-linux-gnueabihf" ;;
        *) boot_fail "no portable Python for the CPU type '$arch' on Linux." ;;
      esac ;;
    Darwin)
      case "$arch" in
        x86_64) triple="x86_64-apple-darwin" ;;
        arm64)  triple="aarch64-apple-darwin" ;;
        *) boot_fail "no portable Python for the CPU type '$arch' on macOS." ;;
      esac ;;
    *) boot_fail "no portable Python for the operating system '$os'." ;;
  esac

  # SHA-256 of the install_only tarballs, from the SHA256SUMS file of release $PBS_TAG.
  case "$triple" in
    x86_64-unknown-linux-gnu)      PY_SHA="6a79e922e50006b601f29e688407659da1c2b86ebcd4e9c7254cf234be8b89de" ;;
    aarch64-unknown-linux-gnu)     PY_SHA="a945427411dc6851a1393533b74a4a1ac434321ae397f2ec69897a9b4fde032b" ;;
    armv7-unknown-linux-gnueabihf) PY_SHA="4cb40be706cb5416342cc179f334af63ac0b88215bb6ecc41cce4ba34e0ae62d" ;;
    x86_64-apple-darwin)           PY_SHA="a962e47df522889eaa7ff0d6a56203f1079f68aabb65ffdbe8455f362bc769f3" ;;
    aarch64-apple-darwin)          PY_SHA="fbf4afef53c2a6b72bb741bc06d5671a9dfaf87bd84e5aa73ac9b3d0d589f14e" ;;
  esac

  asset="cpython-${PY_VER}+${PBS_TAG}-${triple}-install_only.tar.gz"
  url="$PBS_BASE/cpython-${PY_VER}%2B${PBS_TAG}-${triple}-install_only.tar.gz"
  parent="$(runtime_parent)"
  final="$parent/python-${PY_VER}"

  echo "Downloading portable Python $PY_VER ($triple, about 65 MB) from github.com into"
  echo "  $final"
  echo "This happens once; later runs reuse it. Installing Python 3.9+ yourself also works."

  mkdir -p "$parent" 2>/dev/null || boot_fail "cannot create $parent"
  BOOT_TMP="$(mktemp -d "$parent/.bootstrap.XXXXXX" 2>/dev/null)" || boot_fail "cannot create a temporary folder in $parent"
  trap boot_cleanup EXIT
  trap 'boot_cleanup; exit 130' INT TERM

  if command -v curl >/dev/null 2>&1; then
    curl -fL --retry 2 --progress-bar -o "$BOOT_TMP/$asset" "$url" || boot_fail "the download from $url failed."
  elif command -v wget >/dev/null 2>&1; then
    wget -q --show-progress -O "$BOOT_TMP/$asset" "$url" || boot_fail "the download from $url failed."
  else
    boot_fail "neither curl nor wget was found, so the portable Python cannot be downloaded."
  fi

  local got
  if command -v sha256sum >/dev/null 2>&1; then
    got="$(sha256sum "$BOOT_TMP/$asset" | cut -d ' ' -f 1)"
  elif command -v shasum >/dev/null 2>&1; then
    got="$(shasum -a 256 "$BOOT_TMP/$asset" | cut -d ' ' -f 1)"
  else
    boot_fail "neither sha256sum nor shasum was found, so the download cannot be verified."
  fi
  if [[ "$got" != "$PY_SHA" ]]; then
    boot_fail "SHA-256 mismatch for $asset (expected $PY_SHA, got $got); the download was discarded."
  fi
  echo "SHA-256 verified. Unpacking..."

  mkdir "$BOOT_TMP/unpack" && tar -xzf "$BOOT_TMP/$asset" -C "$BOOT_TMP/unpack" || boot_fail "could not unpack $asset."
  [[ -x "$BOOT_TMP/unpack/python/bin/python3" ]] || boot_fail "the archive has no python/bin/python3."

  # A leftover folder here is a runtime that failed its check in step 2.
  rm -rf "$final"
  if ! mv "$BOOT_TMP/unpack" "$final"; then
    # Another run may have finished first; use its result when it works.
    [[ -x "$final/python/bin/python3" ]] || boot_fail "could not move the runtime into $final."
  fi
  boot_cleanup
  trap - EXIT INT TERM

  if ! use_cached_python "$@"; then
    boot_fail "the portable Python in $final does not start."
  fi
}

for candidate in python3 python; do
  if command -v "$candidate" >/dev/null 2>&1 &&
     "$candidate" -c "$PY_MIN_CHECK" >/dev/null 2>&1; then
    exec "$candidate" "$TOOL" "$@"
  fi
done

use_cached_python "$@"
download_python "$@"
exit 1
