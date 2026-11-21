#!/bin/sh
# Run one headless-Chromium UI check script against a bench unit, on the NAS (Robert 2026-11-21).
#   uicheck.sh <run> <unit-ip> <script under bin/, e.g. ui/task2982.mjs>
# Output (facts.json, screenshots, console.log) lands in /volume1/docker/otgw-bench/runs/<run>/.
BASE=/volume1/docker/otgw-bench; R=$BASE/runs/$1; mkdir -p "$R"; chmod 777 "$R"
echo "$(date '+%F %T') uicheck ip=$2 script=$3" >> "$R/run.log"
/usr/local/bin/docker run --rm --network host --shm-size=256m -e TZ=Europe/Amsterdam \
  -v "$BASE/bin/$3:/work/check.mjs:ro" -v "$R:/out" otgw-bench-chromium:alpine3.21 \
  node /work/check.mjs "$2" /out > "$R/console.log" 2>&1
echo "exit $?" | tee -a "$R/run.log"
