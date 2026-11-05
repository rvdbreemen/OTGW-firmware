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

# One tag per published build (maintainer 2026-10-31): refuse a tag that was already published or is not
# newer than the newest published one. The list is machine-wide, shared by every worktree and session.
PUBLISHED="${OTGW_PUBLISHED_TAGS:-${LOCALAPPDATA:-$HOME/.local/share}/OTGW-capture/published-tags.txt}"
# Entries are "<core>-<prerelease>" (2.0.1-alpha.2); older entries carry only the prerelease (alpha.597) and
# count as core 2.0.0, the line they were published on. The order check runs per core, so 2.0.1-alpha.1
# follows 2.0.0-alpha.597 and 2.0.0-beta.1 (Robert 2026-11-01: dev builds after beta.1 are 2.0.1-alpha.N).
core=${version%%-*}
key="$core-$pre"
num=${pre##*.}
if [ -f "$PUBLISHED" ]; then
  norm=$(tr -d '\r' < "$PUBLISHED" | sed -e '/^[0-9]*\.[0-9]*\.[0-9]*-/!s/^/2.0.0-/')
  printf '%s\n' "$norm" | grep -qx "$key" && refuse "prerelease $key was already published: bump (bin/bump-prerelease.sh), commit, rebuild"
  word=${pre%.*}
  maxn=$(printf '%s\n' "$norm" | sed -n "s/^$core-$word\.\([0-9]*\)$/\1/p" | sort -n | tail -1)
  if [ -n "$maxn" ] && [ "$num" -le "$maxn" ] 2>/dev/null; then
    refuse "prerelease $key is not newer than the newest published $core-$word.$maxn"
  fi
fi

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
# JSON payload, ASCII-only (TASK-1610): non-ASCII characters become \uXXXX escapes, because the payload
# reaches the Windows-native curl.exe as a command-line argument, which Windows converts through the
# ANSI code page ("Andre" with an accent arrived as "Andr?"). The text goes to Python on stdin as raw
# UTF-8 bytes, so no argv conversion touches it. Control characters (newlines, tabs) are dropped.
payload=$(printf '%s' "$content" | python -I -c 'import json,sys; t=sys.stdin.buffer.read().decode("utf-8","replace"); t="".join(c for c in t if ord(c)>=32); print(json.dumps({"content":t,"allowed_mentions":{"parse":[]}},ensure_ascii=True))')
[ -n "$payload" ] || refuse "could not build the JSON payload (python missing?)"

# Discord drops '+' from attachment names (alpha.465+5cf2758 showed as alpha.4655cf2758),
# so upload under a name with '_'; the local zip keeps its name.
upload_name=$(printf '%s' "$name" | tr '+' '_')

if [ "$dry_run" -eq 1 ]; then
  echo "DRY RUN OK: would post $name ($size bytes) as $upload_name"
  echo "payload: $payload"
  exit 0
fi

[ -r "$WEBHOOK_FILE" ] || refuse "webhook file not readable: $WEBHOOK_FILE"
url=$(tr -d ' \r\n' < "$WEBHOOK_FILE")
case "$url" in
  https://discord.com/api/webhooks/*) ;;
  *) refuse "webhook file does not hold a discord.com webhook URL" ;;
esac

# Native Windows curl (Git Bash/MSYS) cannot open an MSYS path such as /d/...
# inside a -F argument: MSYS does not convert it there. Hand it a Windows path.
curl_zip="$zip"
if command -v cygpath >/dev/null 2>&1; then curl_zip=$(cygpath -w "$zip"); fi

resp=$(mktemp)
trap 'rm -f "$resp"' EXIT
code=$(curl -sS -o "$resp" -w '%{http_code}' --max-time 120 -H "User-Agent: $UA" \
  --form-string "payload_json=$payload" \
  -F "files[0]=@$curl_zip;type=application/zip;filename=$upload_name" \
  "$url?wait=true") || { echo "FAILED: curl error" >&2; exit 1; }

case "$code" in
  2??) ;;
  *) echo "FAILED: http $code: $(head -c 300 "$resp")" >&2; exit 1 ;;
esac

grep -q "\"size\": *$size[,}]" "$resp" \
  || { echo "FAILED: http $code but no attachment of $size bytes in the reply" >&2; exit 1; }

msg_id=$(python -I -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id",""))' "$resp" 2>/dev/null || true)
echo "POSTED: $name ($size bytes), http $code, message ${msg_id:-unknown}"
mkdir -p "$(dirname "$PUBLISHED")" && echo "$key" >> "$PUBLISHED"   # one tag per published build (core-prerelease)
# Post log next to it (TASK-1632): lets scripts/alpha_ledger.py rebuild the alpha ledger.
# Columns: UTC time, tag key, version+hash, Discord message id, the -m text (tabs and newlines removed).
POSTLOG="${OTGW_POST_LOG:-$(dirname "$PUBLISHED")/published-posts.tsv}"
printf '%s\t%s\t%s\t%s\t%s\n' "$(date -u +%Y-%m-%dT%H:%MZ)" "$key" "$version+$hash" "${msg_id:-unknown}" \
  "$(printf '%s' "$extra" | tr '\t\r\n' '   ')" >> "$POSTLOG"
