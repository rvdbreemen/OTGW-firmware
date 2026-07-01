#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# device-verify.sh (TASK-976) — OTGW on-device build+flash helper.
#
# Builds the LittleFS image (and optionally the firmware), flashes the bench
# ESP32-S3 over USB, and confirms it boots — printing a COMPACT summary only.
# The multi-KB PlatformIO build log and the esptool write-progress dump are
# swallowed into a temp log so they never bloat an agent / session context
# (that dump alone was ~9 KB of noise per flash). Exit 0 = all good.
#
# Usage:
#   bash scripts/device-verify.sh                 # buildfs + flash littlefs
#   bash scripts/device-verify.sh --firmware      # firmware + littlefs (collision-safe)
#   bash scripts/device-verify.sh --firmware-only # firmware only
#   OTGW_IP=192.168.1.143 OTGW_PORT=COM8 bash scripts/device-verify.sh
# ---------------------------------------------------------------------------
set -uo pipefail

# esptool prints a Unicode progress bar; force UTF-8 so redirecting its stdout to
# a log on Windows does not blow up with a cp1252 'charmap' UnicodeEncodeError.
export PYTHONIOENCODING=utf-8

IP="${OTGW_IP:-192.168.88.39}"
PORT="${OTGW_PORT:-COM4}"
BUILD_FW=0
FW_ONLY=0
for a in "$@"; do
  case "$a" in
    --firmware)      BUILD_FW=1 ;;
    --firmware-only) BUILD_FW=1; FW_ONLY=1 ;;
    --ip=*)          IP="${a#*=}" ;;
    --port=*)        PORT="${a#*=}" ;;
    *) echo "unknown arg: $a" >&2; exit 2 ;;
  esac
done

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
PY="${PLATFORMIO_PYTHON:-$HOME/.platformio/penv/Scripts/python.exe}"
LOG="$(mktemp 2>/dev/null || echo /tmp/device-verify.$$.log)"
trap 'rm -f "$LOG" "$LOG.fs"' EXIT

fail() { echo "FAILED"; grep -iE "error|fatal|undefined reference|FAILED" "$LOG" | tail -6; exit 1; }

# --- JS syntax gate (cheap, catches the common data-asset break) ------------
if node --check src/OTGW-firmware/data/v2.js >/dev/null 2>&1; then
  echo "v2.js syntax     : OK"
else
  echo "v2.js syntax     : FAIL"; node --check src/OTGW-firmware/data/v2.js; exit 1
fi

# --- build ------------------------------------------------------------------
# NOTE: running `-t buildfs` after a firmware build wipes firmware.bin (shared
# .pio/build/esp32 dir). So for --firmware we build fs FIRST, stash littlefs.bin,
# build firmware, then flash both from the stash. (bug-034 class collision.)
if [ "$FW_ONLY" = 0 ]; then
  echo -n "buildfs          : "
  "$PY" -m platformio run -e esp32 -t buildfs >"$LOG" 2>&1 || fail
  grep -qiE "esp32 +SUCCESS" "$LOG" || { echo "NO SUCCESS LINE"; tail -5 "$LOG"; exit 1; }
  cp .pio/build/esp32/littlefs.bin "$LOG.fs"
  echo "SUCCESS"
fi
if [ "$BUILD_FW" = 1 ]; then
  echo -n "build firmware   : "
  "$PY" -m platformio run -e esp32 >"$LOG" 2>&1 || fail
  grep -qiE "esp32 +SUCCESS" "$LOG" || { echo "NO SUCCESS LINE"; tail -5 "$LOG"; exit 1; }
  echo "SUCCESS"
fi

# --- flash (suppress esptool progress; keep only the verify count) ----------
FLASH_ARGS=()
[ "$BUILD_FW" = 1 ] && FLASH_ARGS+=(0x10000 .pio/build/esp32/firmware.bin)
[ "$FW_ONLY" = 0 ]  && FLASH_ARGS+=(0x270000 "$LOG.fs")
echo -n "flash ($PORT) : "
"$PY" -m esptool --chip esp32s3 --port "$PORT" --baud 921600 \
  --before default-reset --after hard-reset write-flash "${FLASH_ARGS[@]}" >"$LOG" 2>&1 || fail
echo "OK ($(grep -c "Hash of data verified" "$LOG") image(s) verified)"

# --- confirm boot -----------------------------------------------------------
echo -n "device boot      : "
for i in 1 2 3 4 5 6 7 8; do
  sleep 3
  V="$(curl.exe -s -m 6 "http://$IP/api/v2/device/info" 2>/dev/null \
        | "$PY" -c "import sys,json;print(json.load(sys.stdin)['device']['fwversion'])" 2>/dev/null)"
  [ -n "${V:-}" ] && { echo "UP ($V)"; exit 0; }
done
echo "NOT RESPONDING at $IP (still booting? wrong IP?)"; exit 1
