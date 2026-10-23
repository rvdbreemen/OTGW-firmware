#!/usr/bin/env bash
# Post the esp32-combo flash zip to #alpha-testing through a Discord webhook (TASK-1313).
#
# Usage: scripts/discord-post-build.sh [--dry-run] [-m "extra text"] [zip]
#   zip        defaults to the newest build/OTGW-firmware-esp32-combo-*-flash.zip
#   -m TEXT    appended to the message (e.g. the task id and a one-line summary)
#   --dry-run  run every check, post nothing
#
# The webhook URL is read from $OTGW_DISCORD_WEBHOOK_FILE, default
# %LOCALAPPDATA%\OTGW-capture\discord-alpha-webhook.url (outside the repo). It is a
# secret: this script never prints it.
#
# Refuses (exit 2) when the zip githash is not HEAD, when src/ has uncommitted
# changes (build stamps in version.h / data/version.hash excepted), or when the zip
# exceeds the 10 MB upload limit. Exit 1 on a failed post.
set -euo pipefail

REPO="${OTGW_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
WEBHOOK_FILE="${OTGW_DISCORD_WEBHOOK_FILE:-${LOCALAPPDATA:-$HOME/.local/share}/OTGW-capture/discord-alpha-webhook.url}"
MAX_BYTES=10000000
UA="DiscordBot (https://github.com/rvdbreemen/OTGW-firmware, 1.0)"

dry_run=0; extra=""; zip=""
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) dry_run=1 ;;
    -m) shift; extra="${1:-}" ;;
    -h|--help) sed -n '2,15p' "$0"; exit 0 ;;
    *) zip="$1" ;;
  esac
  shift
done

refuse() { echo "REFUSED: $*" >&2; exit 2; }

if [ -z "$zip" ]; then
  zip=$(ls -t "$REPO"/build/OTGW-firmware-esp32-combo-*-flash.zip 2>/dev/null | head -n 1 || true)
  [ -n "$zip" ] || refuse "no build/OTGW-firmware-esp32-combo-*-flash.zip found"
fi
[ -f "$zip" ] || refuse "zip not found: $zip"
name=$(basename "$zip")

# OTGW-firmware-esp32-combo-<semver>+<hash>-flash.zip
version=$(printf '%s' "$name" | sed -n 's/^OTGW-firmware-esp32-combo-\(.*\)+[0-9a-f]*-flash\.zip$/\1/p')
hash=$(printf '%s' "$name" | sed -n 's/^.*+\([0-9a-f]\{7,40\}\)-flash\.zip$/\1/p')
[ -n "$version" ] && [ -n "$hash" ] || refuse "unexpected zip name: $name"

head_full=$(git -C "$REPO" rev-parse HEAD)
case "$head_full" in
  "$hash"*) ;;
  *) refuse "zip githash $hash is not HEAD ${head_full:0:9}; rebuild from HEAD" ;;
esac

# version.h is excluded from the dirty check (build stamps), so compare the tag with HEAD's
pre=$(git -C "$REPO" show HEAD:src/OTGW-firmware/version.h 2>/dev/null \
  | sed -n 's/^#define _VERSION_PRERELEASE[[:space:]]*//p' | tr -d ' "\r')
[ -n "$pre" ] || refuse "no _VERSION_PRERELEASE in HEAD:src/OTGW-firmware/version.h"
case "$version" in
  *-"$pre") ;;
  *) refuse "zip version $version does not carry HEAD's prerelease $pre (uncommitted bump?)" ;;
esac

dirty=$(git -C "$REPO" status --porcelain -- src \
  ':(exclude)src/OTGW-firmware/version.h' ':(exclude)src/OTGW-firmware/data/version.hash')
if [ -n "$dirty" ]; then
  printf '%s\n' "$dirty" >&2
  refuse "uncommitted changes under src/: the zip would not match its githash"
fi

size=$(wc -c < "$zip" | tr -d ' ')
[ "$size" -le "$MAX_BYTES" ] || refuse "$name is $size bytes, above the $MAX_BYTES byte upload limit"

content="Build ready: \`$version+$hash\` (esp32-combo, flash package attached)."
[ -n "$extra" ] && content="$content $extra"
# JSON string: drop control characters (newlines, tabs), escape backslash and quote
esc=$(printf '%s' "$content" | tr -d '\000-\037' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g')
payload="{\"content\":\"$esc\",\"allowed_mentions\":{\"parse\":[]}}"

if [ "$dry_run" -eq 1 ]; then
  echo "DRY RUN OK: would post $name ($size bytes)"
  echo "payload: $payload"
  exit 0
fi

[ -r "$WEBHOOK_FILE" ] || refuse "webhook file not readable: $WEBHOOK_FILE"
url=$(tr -d ' \r\n' < "$WEBHOOK_FILE")
case "$url" in
  https://discord.com/api/webhooks/*) ;;
  *) refuse "webhook file does not hold a discord.com webhook URL" ;;
esac

resp=$(mktemp)
trap 'rm -f "$resp"' EXIT
code=$(curl -sS -o "$resp" -w '%{http_code}' --max-time 120 -H "User-Agent: $UA" \
  --form-string "payload_json=$payload" \
  -F "files[0]=@$zip;type=application/zip" \
  "$url?wait=true") || { echo "FAILED: curl error" >&2; exit 1; }

case "$code" in
  2??) ;;
  *) echo "FAILED: http $code: $(head -c 300 "$resp")" >&2; exit 1 ;;
esac

grep -q "\"size\": *$size[,}]" "$resp" \
  || { echo "FAILED: http $code but no attachment of $size bytes in the reply" >&2; exit 1; }

msg_id=$(python -I -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id",""))' "$resp" 2>/dev/null || true)
echo "POSTED: $name ($size bytes), http $code, message ${msg_id:-unknown}"
