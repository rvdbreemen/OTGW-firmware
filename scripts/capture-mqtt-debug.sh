#!/usr/bin/env bash
# capture-mqtt-debug.sh - OTGW diagnostic capture launcher (Linux and macOS).
#
# The capture itself is otgw_capture.py next to this file (ADR-202). This
# launcher only finds a Python 3.9+ interpreter and runs it with every argument.
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

for candidate in python3 python; do
  if command -v "$candidate" >/dev/null 2>&1 &&
     "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)' >/dev/null 2>&1; then
    exec "$candidate" "$TOOL" "$@"
  fi
done

echo "Python 3.9 or newer was not found." >&2
echo "  macOS:         xcode-select --install   (or: brew install python)" >&2
echo "  Debian/Ubuntu: sudo apt install python3" >&2
echo "  Fedora:        sudo dnf install python3" >&2
exit 1
