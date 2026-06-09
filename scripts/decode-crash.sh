#!/usr/bin/env bash
# decode-crash.sh — decode an ESP8266 panic dump against a firmware .elf.
# Companion to capture-usb-serial.bat (TASK-836): turns the >>>stack>>> return
# addresses into file:line/function names so we can pin the faulting caller.
#
# Usage:
#   scripts/decode-crash.sh <crash-frames.log | transcript.txt> <firmware.elf>
#
# The .elf MUST be the exact build that was flashed (same git commit + same
# build-time core patch state). For beta.4 (HEAD 71cd44a) a matching elf is at
# bisect-testset/beta4-decode/OTGW-firmware-1.7.0-beta.4.elf.
set -uo pipefail

LOG="${1:?usage: decode-crash.sh <log> <elf>}"
ELF="${2:?usage: decode-crash.sh <log> <elf>}"
[ -f "$LOG" ] || { echo "log not found: $LOG" >&2; exit 1; }
[ -f "$ELF" ] || { echo "elf not found: $ELF" >&2; exit 1; }

A2L=$(find arduino -iname "xtensa-lx106-elf-addr2line*" 2>/dev/null | head -1)
[ -n "$A2L" ] || { echo "xtensa addr2line not found under arduino/" >&2; exit 1; }

echo "=== exception registers (epc1 in 0x4000xxxx = ROM, not in elf) ==="
grep -aoE "(epc1|epc2|epc3|excvaddr|depc)=0x[0-9a-fA-F]+" "$LOG" | sort -u

echo
echo "=== decoded code addresses (0x402xxxxx = flash text; the call chain) ==="
grep -aoE "0x40[12][0-9a-fA-F]{5}" "$LOG" | tr 'A-F' 'a-f' | sort -u | while read -r addr; do
  printf "%s  " "$addr"
  "$A2L" -pfiaC -e "$ELF" "$addr" 2>/dev/null
done
