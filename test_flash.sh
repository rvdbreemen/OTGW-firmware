#!/usr/bin/env bash
set -e

echo "Testing find_first_match function..."

find_first_match() {
    # Intentional glob expansion: $1 contains a wildcard pattern.
    for f in $1; do
        [ -f "$f" ] && echo "$f"
    done | sort | head -n 1
}

SCRIPT_DIR="$(pwd)"
echo "SCRIPT_DIR: $SCRIPT_DIR"

FW_FILE=""
FS_FILE=""

for dir in "$SCRIPT_DIR" "$SCRIPT_DIR/build"; do
    echo "Checking directory: $dir"
    [ -d "$dir" ] || { echo "  Directory doesn't exist"; continue; }
    
    if [ -z "$FW_FILE" ]; then
        echo "  Looking for firmware in $dir..."
        FW_FILE="$(find_first_match "$dir/OTGW-firmware-*.ino.bin")"
        echo "  FW_FILE result: '$FW_FILE'"
    fi
    
    if [ -z "$FS_FILE" ]; then
        echo "  Looking for filesystem in $dir..."
        FS_FILE="$(find_first_match "$dir/OTGW-firmware*.littlefs.bin")"
        echo "  FS_FILE result: '$FS_FILE'"
    fi
done

echo
echo "Final results:"
echo "FW_FILE: '$FW_FILE'"
echo "FS_FILE: '$FS_FILE'"

if [ -z "$FW_FILE" ] || [ -z "$FS_FILE" ]; then
    echo "Binaries not found - would prompt user"
else
    echo "Binaries found - would proceed to flash"
fi
