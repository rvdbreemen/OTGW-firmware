---
id: TASK-393
title: Port TASK-392 HA discovery fixes from dev to 2.0.0
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 00:09'
updated_date: '2026-04-24 00:14'
labels:
  - port
  - mqtt
  - homeassistant
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Cherry-pick the meaningful commits from dev (1a87b784 fix + 63170ad7 review archive) onto feature-dev-2.0.0-otgw32-esp32-sat-support. The dev-side version bump (58115150) is deliberately skipped because it carries a 1.4.2-beta version string which is wrong for 2.0.0. After cherry-pick, verify evaluator and firmware build still pass on 2.0.0 with its own data model and ADR-077 streaming discovery architecture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Cherry-pick 1a87b784 (HA ID 247 fix + sanitizer + BREAKING_CHANGES + FSexplorer + evaluator gate) applies cleanly onto 2.0.0, resolving expected version.h/version.hash conflicts in favor of 2.0.0
- [x] #2 Cherry-pick 63170ad7 (docs/reviews archive) applies cleanly on 2.0.0
- [x] #3 python evaluate.py --quick passes including the new HA-DISC sensor index consistency gate, or if it fails on 2.0.0 due to rename/data-model drift the failure is understood and addressed
- [x] #4 python build.py --firmware compiles cleanly on 2.0.0, no sanitizeHaObjectId name collision, no broken helpers
- [x] #5 Version bump commit 58115150 is deliberately NOT ported; the next natural 2.0.0 build.py run produces a correct 2.0.0-beta version string
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. git cherry-pick 1a87b784 (the fix). Expected auto-merges: 4 files clean. Expected conflicts: version.h + version.hash (keep 2.0.0 side). Check whether rename mqtt_configuratie.cpp -> MQTTHaDiscovery.cpp was handled correctly.
2. Inspect the cherry-picked file; verify mqttHaSensorIndex[247] now equals 289 (or the correct 2.0.0 value) and sanitizeHaObjectId helper landed in the right location.
3. git cherry-pick 63170ad7 (archive).
4. python evaluate.py --quick: confirm HA-DISC gate runs. If it fails due to renamed array or different data model, fix regex and note.
5. python build.py --firmware in background. Verify no symbol collisions / warnings.
6. git status + git log --oneline -5 to confirm clean state. Push to origin/feature-dev-2.0.0-... after both gates pass.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Cherry-pick 1a87b784 completed cleanly (commit 79dea8dd). Git rename-detection mapped mqtt_configuratie.cpp -> MQTTHaDiscovery.cpp automatically; index-247 fix, sanitizeHaObjectId helper, and both callsites (composeSensorPayload, buildSensorDiscoveryTopic) landed in the right places. Verified: MQTTHaDiscovery.cpp:1335 now reads "289, // id 247, 17 entries", helper at line 1842, callsites at 1864 and 2041. 17 id-247 rows exist on 2.0.0, same as dev, so 289 is correct.
Cherry-pick 63170ad7 (archive) applied cleanly as commit 4d89587c.
Version bump commit 58115150 deliberately skipped; next python build.py correctly bumped to 2.0.0-beta+4d89587 (build 3490).
evaluate.py HA-DISC gate originally WARNed "mqtt_configuratie.cpp not found" because of the rename. Fixed by adding a fallback lookup to MQTTHaDiscovery.cpp. Gate now runs on 2.0.0 and is silent (PASS) when re-run. Same code stays PASS on dev (that branch has mqtt_configuratie.cpp). No divergence between branches.
Firmware build: python build.py --firmware exit 0. Dual-platform artifacts built: ESP32 1.80 MB .ino.bin + 1.00 MB littlefs, ESP8266 0.77 MB .ino.bin + 1.98 MB littlefs. No warnings, no errors. sanitizeHaObjectId helper compiles cleanly under both ESP32 and ESP8266 cores.
Pre-existing non-port issue: 1 PROGMEM FAIL on 2.0.0 (15 violations elsewhere in the codebase). Not introduced by this port; left as standing 2.0.0 issue.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the HA discovery + related fixes from TASK-392 on dev onto feature-dev-2.0.0-otgw32-esp32-sat-support.

Changes:
- Cherry-picked 1a87b784 (HA ID 247 index fix, sanitizeHaObjectId helper, BREAKING_CHANGES.md upgrade order, FSexplorer protectedFiles, new HA-DISC evaluator gate). Git rename-detection automatically mapped mqtt_configuratie.cpp -> MQTTHaDiscovery.cpp so the fix landed in the correct 2.0.0 file without manual intervention.
- Cherry-picked 63170ad7 (docs/reviews archive of the v1.4.1..dev review findings).
- Deliberately skipped 58115150 (version bump) because it carried a 1.4.2-beta version string wrong for 2.0.0; the next build.py run bumped the branch to its own 2.0.0-beta+4d89587.
- Additional small fix: made the new HA-DISC evaluator gate portable across the rename. It now tries mqtt_configuratie.cpp first (dev name) and falls back to MQTTHaDiscovery.cpp (2.0.0 name), so the same code works on either branch without divergence.

Tests / verification:
- Source inspection: MQTTHaDiscovery.cpp:1335 has "289, // id 247, 17 entries"; sanitizeHaObjectId at line 1842; callsites at 1864 and 2041.
- python build.py --firmware: exit 0, dual-platform artifacts (ESP32 + ESP8266), no warnings, no errors.
- python evaluate.py --quick: HA-DISC gate PASSES on 2.0.0. Only remaining FAIL is the pre-existing 15 PROGMEM violations which pre-date this port.

Risks / follow-ups:
- The pre-existing PROGMEM FAIL on 2.0.0 is a standing 2.0.0 issue and was not addressed here (out of scope for a port task).
- The gate-portability improvement should also be backported to dev at some point so both branches have identical HA-DISC behaviour; for now dev works fine because mqtt_configuratie.cpp still exists there.
- Runtime smoke-test on a real ESP32/OTGW32 device: verify the 17 retained homeassistant/sensor/<nodeId>/otgw-firmware_stats_* discovery topics appear once 2.0.0 is flashed.
<!-- SECTION:FINAL_SUMMARY:END -->
