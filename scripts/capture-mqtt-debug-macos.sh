#!/usr/bin/env bash
# Compatibility wrapper. The canonical macOS/Linux launcher is
# capture-mqtt-debug.sh so it shares the Windows script basename.

set -u

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/capture-mqtt-debug.sh" "$@"
