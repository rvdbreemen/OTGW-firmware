#!/usr/bin/env bash
set -e
set -u
set -o pipefail

echo "Testing glob expansion..."

find_first_match() {
    for f in $1; do
        [ -f "$f" ] && echo "$f"
    done | sort | head -n 1
}

SCRIPT_DIR="$(pwd)"
FW_FILE=""

echo "Looking for: $SCRIPT_DIR/OTGW-firmware-*.ino.bin"
FW_FILE="$(find_first_match "$SCRIPT_DIR/OTGW-firmware-*.ino.bin")"
echo "Result: '$FW_FILE'"

if [ -z "$FW_FILE" ]; then
    echo "FW_FILE is empty - this is expected"
else
    echo "FW_FILE has content: $FW_FILE"
fi

echo "Script completed successfully"
