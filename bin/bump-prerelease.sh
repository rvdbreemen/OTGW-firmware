#!/usr/bin/env bash
set -euo pipefail
# Increment _VERSION_PRERELEASE in src/OTGW-firmware/version.h.
# Tag must match ^[a-zA-Z]+\.[0-9]+$ (current usage: alpha.6, beta.NN).

ROOT=$(git rev-parse --show-toplevel)
VERSION_H="$ROOT/src/OTGW-firmware/version.h"
[ -f "$VERSION_H" ] || { echo "bump-prerelease: $VERSION_H not found (run from project root)" >&2; exit 1; }

CURRENT=$(grep -E '^#define _VERSION_PRERELEASE ' "$VERSION_H" | awk '{print $3}')
[ -n "$CURRENT" ] || { echo "bump-prerelease: _VERSION_PRERELEASE not found in version.h" >&2; exit 1; }

if [[ ! "$CURRENT" =~ ^([a-zA-Z]+)\.([0-9]+)$ ]]; then
  echo "bump-prerelease: tag '$CURRENT' does not match ^[a-zA-Z]+\\.[0-9]+\$ (expected e.g. beta.23 or alpha.6)" >&2
  exit 1
fi
WORD="${BASH_REMATCH[1]}"
N="${BASH_REMATCH[2]}"
NEW="${WORD}.$((N + 1))"

# --update-all keeps every source-file version banner in sync with version.h;
# --print-updated lists each modified file so the whole bump can be staged in one go.
UPDATED=$(python3 "$ROOT/scripts/autoinc-semver.py" "$ROOT/src/OTGW-firmware" --prerelease "$NEW" --update-all --print-updated)

STAGED=0
while IFS= read -r f; do
  f=${f%$'\r'}   # strip CR from Windows Python output
  [ -n "$f" ] || continue
  git -C "$ROOT" add "src/OTGW-firmware/$f"
  STAGED=$((STAGED + 1))
done <<< "$UPDATED"

echo "$CURRENT → $NEW (staged $STAGED files)"
