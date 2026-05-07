#!/usr/bin/env bash
# Increment _VERSION_PRERELEASE in src/OTGW-firmware/version.h.
# Tag must match ^[a-zA-Z]+\.[0-9]+$ (current usage: beta.23, alpha.6).
# Calls scripts/autoinc-semver.py --prerelease for the rewrite.
# Does NOT git-add — caller decides whether to stage.
#
# Usage: bin/bump-prerelease.sh

set -euo pipefail

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

python3 "$ROOT/scripts/autoinc-semver.py" "$ROOT/src/OTGW-firmware" --prerelease "$NEW" >/dev/null
echo "$CURRENT → $NEW"
