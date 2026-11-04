#!/bin/bash
# SessionStart hook: pre-provision the ESP firmware build toolchain so Claude
# Code on the web sessions can run `python build.py --firmware` and self-verify
# build-gated tasks.
#
# The toolchain download (arduino-cli / PlatformIO platform + xtensa toolchain)
# is fetched from downloads.arduino.cc, github.com release assets and
# arduino.esp8266.com. Those hosts must be permitted by the environment's
# network policy for this hook to succeed; until then it degrades gracefully
# and never blocks session start.
set -uo pipefail

# Local (non-web) runs already have a working toolchain — do nothing.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
MARKER="${HOME}/.cache/otgw-toolchain-ready"
LOG="/tmp/otgw-toolchain-setup.log"

# Idempotent: container state is cached after the hook completes, so once a
# session has provisioned the toolchain later sessions skip the slow path.
if [ -f "$MARKER" ]; then
  echo "OTGW build toolchain already provisioned (cached) — skipping."
  exit 0
fi

echo "Provisioning OTGW ESP build toolchain via build.py (first web session may take several minutes)..."

# build.py stamps _VERSION_BUILD / _VERSION_GITHASH into version.h and data/version.hash.
# That build stamp is not a version bump (CLAUDE.md), so restore it afterwards, but only
# when both files were clean before this build and no _VERSION_PRERELEASE line changed:
# an earlier edit or a real bump in the session's tree is never touched (TASK-1628).
VERSION_FILES=("src/OTGW-firmware/version.h" "src/OTGW-firmware/data/version.hash")
stamp_was_clean=0
if git -C "$PROJECT_DIR" diff --quiet -- "${VERSION_FILES[@]}" 2>/dev/null; then
  stamp_was_clean=1
fi
restore_build_stamp() {
  [ "$stamp_was_clean" = 1 ] || return 0
  if git -C "$PROJECT_DIR" diff -U0 -- "${VERSION_FILES[@]}" 2>/dev/null | grep -q '^[-+]#define _VERSION_PRERELEASE'; then
    return 0
  fi
  git -C "$PROJECT_DIR" checkout -- "${VERSION_FILES[@]}" 2>/dev/null || true
}

if python3 "${PROJECT_DIR}/build.py" --firmware >"$LOG" 2>&1; then
  restore_build_stamp
  mkdir -p "$(dirname "$MARKER")"
  : >"$MARKER"
  echo "OTGW build toolchain provisioned; firmware build succeeded."
else
  restore_build_stamp
  {
    echo "WARNING: build.py could not provision the toolchain in this session."
    echo "Most likely the environment network policy blocks the toolchain download"
    echo "(downloads.arduino.cc / github.com + objects.githubusercontent.com release"
    echo "assets / arduino.esp8266.com). Allowlist those hosts in the environment"
    echo "network policy; this hook will then provision and cache the toolchain"
    echo "automatically on the next session."
    echo "Setup log: ${LOG}"
  } >&2
fi

# Never fail session start, regardless of toolchain outcome.
exit 0
