#!/bin/sh
# Usage: scripts/assert-head.sh <hash>
# Exit 0 when <hash> (any length, branch or tag name) is the commit HEAD points at; exit 1 and print both full
# hashes when it is not. Run it before BUILD START: the githash baked into the image must be the HEAD you announced.
# Both sides go through rev-parse, so a 7-character and a 9-character form of the same commit compare equal.
want=$(git rev-parse --verify --quiet "$1^{commit}") || { echo "assert-head: '$1' is not a commit in this repository" >&2; exit 1; }
head=$(git rev-parse --verify --quiet "HEAD^{commit}") || { echo "assert-head: HEAD does not resolve" >&2; exit 1; }
[ "$want" = "$head" ] && exit 0
echo "assert-head: MISMATCH: $1 is $want, HEAD is $head" >&2
exit 1
