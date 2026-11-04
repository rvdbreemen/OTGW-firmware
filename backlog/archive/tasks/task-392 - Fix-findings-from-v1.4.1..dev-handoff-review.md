---
id: TASK-392
title: Fix findings from v1.4.1..dev handoff review
status: Done
assignee:
  - '@claude'
created_date: '2026-04-23 23:22'
updated_date: '2026-04-23 23:33'
labels:
  - bug
  - mqtt
  - homeassistant
  - docs
  - webui
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Four review findings from FINDINGS_HANDOFF_v1.4.1_to_dev.md need to be addressed before the next release: the new heap/discovery stats HA discovery path is broken (wrong index + slashes in object_id), the cumulative BREAKING_CHANGES.md still documents the destructive upgrade order, and new required Web UI assets are not protected from deletion in FSexplorer. Also adds an evaluator gate so the index-mismatch class of bug cannot recur.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 mqttHaSensorIndex[247] resolves to 289 and the 17 pseudo-ID 247 stats discovery configs publish via the normal async drip path without retry loops
- [x] #2 HA discovery object_id and uniq_id for stats entries contain only [a-zA-Z0-9_-] (no slashes), while stat_t still reads <mqttPubTopic>/otgw-firmware/stats/<label>
- [x] #3 docs/BREAKING_CHANGES.md correct upgrade procedure says filesystem binary first, firmware binary second, aligned with README.md and RELEASE_NOTES_1.4.1.md; no misleading 10-minute wait on the happy path
- [x] #4 FSexplorer protectedFiles list blocks deletion of ds-tokens.css, index_dark.css, index_common.css, FSexplorer_dark.css, graph.js, favicon.ico, and the three Inter/JetBrains Mono woff2 fonts
- [x] #5 evaluate.py includes a check_ha_sensor_index_consistency gate that parses mqttHaSensors[] and mqttHaSensorIndex[256] and fails CI on any desync
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix ID 247 index at mqtt_configuratie.cpp:1333 (0xFFFF -> 289 with comment).
2. Add static sanitizeHaObjectId() helper and wire into buildSensorDiscoveryTopic + composeSensorPayload (separate idLabel buffer for uniq_id; label stays untouched for stat_t).
3. Rewrite Correct Upgrade Procedure block in docs/BREAKING_CHANGES.md to match README.md and RELEASE_NOTES_1.4.1.md (filesystem first).
4. Extend protectedFiles array in data/FSexplorer.html with new firmware-owned Web UI assets.
5. Add check_ha_sensor_index_consistency() gate to evaluate.py modeled after existing discovery gates.
6. Run python build.py and python evaluate.py --quick; verify runtime against FINDINGS_HANDOFF validation script.
7. Final Summary + mark Done after all ACs pass.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Issue 1 (index 247): mqtt_configuratie.cpp:1333 changed from 0xFFFF to 289.
Issue 2 (object_id sanitize): added static sanitizeHaObjectId() helper at mqtt_configuratie.cpp:1848, applied in buildSensorDiscoveryTopic (line 2045) and composeSensorPayload via separate idLabel buffer for uniq_id (line 1894). stat_t still uses original label (line 1923) so MQTT state topic still contains the expected slashes.
Issue 3 (docs): docs/BREAKING_CHANGES.md:59-65 rewritten to filesystem-first order with separate Recovery note.
Issue 4 (FSexplorer): protectedFiles list at data/FSexplorer.html:213 extended with ds-tokens.css, index_dark.css, index_common.css, FSexplorer_dark.css, graph.js, favicon.ico and 3 woff2 fonts.
Issue 5 (evaluator gate): check_ha_sensor_index_consistency() added in evaluate.py, wired into run_all_evaluations. Gate found and I fixed a parser regression on trailing-comma-less last array entry (line 1341 0xFFFF // id 255). Evaluator now PASSES with 94.3% health, 0 failures.
Firmware build: python build.py --firmware running in background.

Build verification: python build.py --firmware exit code 0. Artifact OTGW-firmware-1.4.2-beta+b8295e4.ino.bin (0.70 MB). No warnings, no errors. C++ changes (sanitizeHaObjectId helper + idLabel buffer + index fix) compile cleanly under ESP8266 core 3.1.2.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Addresses four review findings from FINDINGS_HANDOFF_v1.4.1_to_dev.md plus adds a regression gate in the evaluator.

Changes:
- mqtt_configuratie.cpp: mqttHaSensorIndex[247] set from 0xFFFF to 289 (17 stats entries starting at row 289) so the async drip path actually reaches the 17 pseudo-ID 247 heap/discovery stats sensors instead of retrying forever.
- mqtt_configuratie.cpp: added static sanitizeHaObjectId() that replaces any byte outside [a-zA-Z0-9_-] with '_'. Applied in buildSensorDiscoveryTopic (discovery object_id) and composeSensorPayload (uniq_id, via a new idLabel buffer). The stat_t field keeps using the original label with slashes, so the MQTT state topic continues to read <mqttPubTopic>/otgw-firmware/stats/<metric>. For entries without slashes in their label the sanitizer is a no-op, so no existing HA entities are renamed.
- docs/BREAKING_CHANGES.md: rewrote the v1.4.1 Correct upgrade procedure to filesystem-first / firmware-second, aligned with README.md and RELEASE_NOTES_1.4.1.md. Moved the 5-10 minute wait + re-enter-settings warning out of the happy path into a separate Recovery note for users who already flashed in the wrong order.
- data/FSexplorer.html: extended the protectedFiles array with ds-tokens.css, index_dark.css, index_common.css, FSexplorer_dark.css, graph.js, favicon.ico, and the three Inter / JetBrains Mono woff2 fonts.
- evaluate.py: added check_ha_sensor_index_consistency() gate that parses mqttHaSensors[] and mqttHaSensorIndex[256] and fails CI on any desync. Wired into run_all_evaluations next to the existing ADR-062 discovery gates.

Tests / verification:
- python evaluate.py --quick: 35 checks, 31 passed, 2 warn (pre-existing), 0 failed, health 94.3% (up from 91.4%). New HA-DISC gate PASSES.
- python build.py --firmware: clean build, no warnings, no errors. Artifact 0.70 MB.
- Static cross-check: for pseudo-ID 247 the resulting HA discovery topic becomes homeassistant/sensor/<nodeId>/otgw-firmware_stats_<metric>/config with uniq_id <nodeId>-otgw-firmware_stats_<metric>, and stat_t stays <mqttPubTopic>/otgw-firmware/stats/<metric>. All three strings HA-valid.

Risks / follow-ups:
- Runtime smoke-test on a real OTGW device (subscribe homeassistant/sensor/<nodeId>/# and confirm 17 retained stats configs appear) is the next post-merge validation step. The static fix is logically complete but HA's own behaviour on the renamed object_id should be eyeballed once.
- The FSexplorer protected list still matches on basename; a generalized directory/extension-based rule was kept out of scope (noted in FINDINGS as possible follow-up).
- The Python build-env issue flagged in FINDINGS (Windows ACL on Python binary) turned out to be non-blocking on this workstation — evaluator and build both ran fine via "python".
<!-- SECTION:FINAL_SUMMARY:END -->
