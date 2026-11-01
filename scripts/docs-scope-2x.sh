#!/usr/bin/env bash
# docs-scope-2x.sh: Phase 1 of /update-docs-2x (TASK-1548).
#
# Lists which documentation is affected by the changes since the previous 2.x
# tag on the current branch. Prints the base tag, the affected doc areas with
# their changed files, the changed files that match no area ("unmapped"), and
# the OTGW32 / ESP8266 mention counts per manual chapter.
#
# Usage:
#   bash scripts/docs-scope-2x.sh                  # standalone / beta: last v2.* tag
#   bash scripts/docs-scope-2x.sh --mode stable    # last v2 stable tag (no '-')
#   bash scripts/docs-scope-2x.sh --base <ref>     # explicit base (tag or commit)
#
# Exit 0 = scope printed, 2 = no usable base (refuses a v1.* base).

set -euo pipefail

MODE=standalone
BASE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --mode) MODE="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

cd "$(git rev-parse --show-toplevel)"

FIRST_STABLE=0
if [ -z "$BASE" ]; then
  # --merged HEAD: tags on release/2.x are not on dev and must not be picked.
  if [ "$MODE" = "stable" ]; then
    BASE=$(git tag --list 'v2.*' --merged HEAD --sort=-v:refname | grep -v -- '-' | head -1 || true)
    if [ -z "$BASE" ]; then
      FIRST_STABLE=1
      BASE=$(git tag --list 'v2.*' --merged HEAD --sort=-v:refname | head -1 || true)
    fi
  else
    BASE=$(git tag --list 'v2.*' --merged HEAD --sort=-v:refname | head -1 || true)
  fi
fi

case "$BASE" in
  "") echo "REFUSE: no v2.* tag reachable from HEAD; pass --base <ref>" >&2; exit 2 ;;
  v1.*) echo "REFUSE: base $BASE is a 1.x tag; 1.x is not an ancestor of the 2.x line" >&2; exit 2 ;;
esac
git rev-parse --verify --quiet "$BASE^{commit}" >/dev/null || { echo "REFUSE: $BASE is not a commit" >&2; exit 2; }

# Area table: area@path regex (extended, matched against the repo path)@docs
AREAS='
network@^src/OTGW-firmware/(networkStuff\.(ino|h)|Ethernet\.ino|Networktypes\.h|NTPtypes\.h)$@ch06/h06, docs/c4/c4-code-network.md
mqtt@^src/OTGW-firmware/(MQTTstuff\.(ino|h)|MQTTHaDiscovery\.cpp|mqtt_discovery_verify\.(cpp|h)|SATmqttPublish\.(cpp|h))$@ch04/h04, docs/api/MQTT.md, docs/c4/c4-code-mqtt.md
rest@^src/OTGW-firmware/(restAPI\.ino|jsonStuff\.ino|jsonEmit\.h|jsonChunked\.h)$@ch09/h09, docs/api/openapi.yaml, docs/api/README.md, docs/c4/c4-code-rest-api.md
websocket@^src/OTGW-firmware/(webSocketStuff\.ino|WsUtf8\.h)$@ch09/h09, docs/api/WEBSOCKET_FLOW.md, docs/api/WEBSOCKET_QUICK_REFERENCE.md
otcore@^src/OTGW-firmware/(OTGW-Core\.(ino|h)|OTDirect(\.ino|types\.h)|OTBustypes\.h|PIC[A-Za-z]*\.h|picSettingValue\.h|otgwSimulationFixture\.h)$@ch08/h08, docs/c4/c4-code-otgw-core.md, docs/c4/c4-code-otdirect.md
sat@^src/OTGW-firmware/SAT[A-Za-z]*\.(ino|h|cpp)$@ch05/h05, docs/c4/c4-code-sat.md, docs/c4/c4-component-smart-thermostat.md
sensors@^src/OTGW-firmware/(sensors_ext\.ino|Sensorstypes\.h|s0PulseCount\.ino|S0types\.h|dhwWaterMeter\.ino|outputs_ext\.ino|Outputstypes\.h|OLED\.ino)$@ch02/h02, docs/api/DALLAS_SENSOR_LABELS_API.md, docs/c4/c4-code-sensors.md
settings@^src/OTGW-firmware/(settingStuff\.ino|settingsHelp\.h|OTGW-firmware\.h|(Device|UI|Uptime|Hardware|Flash)types\.h)$@ch03/h03 (Settings page), docs/c4/c4-code-settings.md
webui@^src/OTGW-firmware/(data/(v2\.(html|js|css)|v2-bundle\.css|ds-tokens\.css|FSexplorer\.html|fsexplorer\.css)|FSexplorer\.ino|updateServerHtml\.h|graphHistoryStuff\.(ino|h))$@ch03/h03, docs/c4/c4-code-web-assets.md
webhook@^src/OTGW-firmware/(webhook\.ino|Webhooktypes\.h)$@ch03/h03, ch09/h09
ota@^(src/OTGW-firmware/OTGW-ModUpdateServer.*\.h|safeboot/.*|partitions_otgw_esp32\.csv|flash_otgw\.(bat|sh)|flash_esp\.py)$@ch02/h02, ch07/h07, docs/c4/c4-code-network.md
platform@^src/libraries/Platform/.*$@ch02/h02 (boards), ch08/h08
build@^(build\.(py|bat|sh)|platformio\.ini|evaluate\.py)$@ch08/h08
capture@^scripts/(capture-.*|otgw_capture\.py|capture-serial\.py)$@ch07/h07
main@^src/OTGW-firmware/(OTGW-firmware\.ino|debugStuff\.(ino|h)|handleDebug\.ino|helperStuff\.(ino|h)|versionStuff\.ino|safeTimers\.h|OTGWLogMacros\.h|webServerCompat\.h)$@ch01/h01 if feature-level, ch08/h08
'

# Files that are never doc triggers by themselves.
IGNORE='^(scripts/sat_.*|scripts/json-golden/.*|scripts/_secrets\.py|src/OTGW-firmware/(version\.h|data/version\.hash)|src/OTGW-firmware/data/(pic16f[0-9]+/.*|fonts/.*|.*\.(png|ico|dat)))$'
# Only scripts/, safeboot/ and src/ matter for the unmapped list; docs, backlog,
# tests and .claude are not firmware behaviour.
SCOPE='^(src/|scripts/|safeboot/|build\.|flash_|partitions_|platformio\.ini|evaluate\.py)'

COMMITS=$(git rev-list --count "$BASE..HEAD")
echo "Base: $BASE ($(git rev-parse --short "$BASE^{commit}"))  Head: $(git rev-parse --short HEAD)  Mode: $MODE  Commits: $COMMITS"
[ "$FIRST_STABLE" = 1 ] && echo "First 2.x stable: no earlier v2 stable tag. Changelog base = $BASE; BREAKING changes and upgrade notes are measured against the 1.x line."

declare -A HITS
UNMAPPED=""
BANNER_ONLY=0
while IFS= read -r f; do
  [ -z "$f" ] && continue
  echo "$f" | grep -Eq "$SCOPE" || continue
  echo "$f" | grep -Eq "$IGNORE" && continue
  # A prerelease bump rewrites the '**  Version  : v...' banner in ~49 files:
  # a file whose only change is that banner is not a doc trigger.
  real=$(git diff "$BASE..HEAD" -- "$f" | grep -E '^[-+]' | grep -Ev '^(\+\+\+|---) ' | grep -Ev 'Version +: v[0-9]' || true)
  if [ -z "$real" ]; then BANNER_ONLY=$((BANNER_ONLY+1)); continue; fi
  matched=0
  while IFS='@' read -r area rx docs; do
    [ -z "$area" ] && continue
    if echo "$f" | grep -Eq "$rx"; then HITS[$area]+="$f "; matched=1; fi
  done <<< "$AREAS"
  [ "$matched" = 0 ] && UNMAPPED+="$f "
done < <(git diff --name-only "$BASE..HEAD")

echo
echo "Affected doc areas:"
n=0
while IFS='@' read -r area rx docs; do
  [ -z "$area" ] && continue
  if [ -n "${HITS[$area]:-}" ]; then
    n=$((n+1))
    cnt=$(echo ${HITS[$area]} | wc -w)
    echo "  $area ($cnt files) -> $docs"
    for f in ${HITS[$area]}; do echo "      $f"; done
  fi
done <<< "$AREAS"
[ "$n" = 0 ] && echo "  (none)"
echo "Areas affected: $n   Banner-only files skipped: $BANNER_ONLY"
[ "$n" -ge 6 ] && echo "Six or more areas changed: treat all manual chapters as affected."

echo
if [ -n "$UNMAPPED" ]; then
  echo "Unmapped changed files (decide per file, then extend the table in this script):"
  for f in $UNMAPPED; do echo "  $f"; done
else
  echo "Unmapped changed files: none"
fi

echo
echo "Manual findings: OTGW32 / ESP8266-Wemos mentions per manual file (TASK-1549 target: OTGW32=0;"
echo "ESP8266 only in 1.x context). A non-zero OTGW32 count is a finding for Robert."
for f in docs/manuals/en/ch*.md docs/manuals/nl/h*.md docs/MANUAL.md; do
  o=$(grep -Eci 'otgw[ _-]?32|ot-thing' "$f" || true)
  e=$(grep -Eci 'esp8266|wemos|d1 mini' "$f" || true)
  printf '  %-40s OTGW32=%-3s ESP8266=%s\n' "$(basename "$f")" "$o" "$e"
done
