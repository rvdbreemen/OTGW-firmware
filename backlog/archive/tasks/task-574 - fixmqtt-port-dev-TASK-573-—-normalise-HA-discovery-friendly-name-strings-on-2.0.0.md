---
id: TASK-574
title: >-
  fix(mqtt): port dev TASK-573 — normalise HA discovery friendly-name strings on
  2.0.0
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 21:24'
updated_date: '2026-05-07 21:29'
labels:
  - mqtt
  - ha-discovery
  - friendly-name
  - andre-feedback
  - polish
  - port-from-dev
dependencies: []
priority: high
ordinal: 34000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of dev TASK-573. 2.0.0 worktree has the same 125 defective friendly-name strings in MQTTHaDiscovery.cpp (separate translation unit per ADR-077 vs dev's mqtt_configuratie.cpp); 2.0.0 also has 7 extra strings (OTDirect + SAT BLE) which already conform. Mapping table is byte-identical to dev (verified via diff /tmp/friendly-rename/dev.tsv /tmp/friendly-rename/twozero.tsv). Same scope guardrail: ONLY friendly-name PROGMEM string values, plus one duplicate variable removal (ha_name_chpumpoperationhoursg).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All 125 shared friendly-name strings in MQTTHaDiscovery.cpp normalised — diff against dev's resulting friendly-name set is empty for shared strings
- [x] #2 Typo'd variable ha_name_chpumpoperationhoursg removed; line 1095 caller retargeted to ha_name_chpumpoperationhours
- [x] #3 Build green: python build.py --firmware exits 0
- [x] #4 Evaluator green: python evaluate.py --quick shows no new failures
- [x] #5 No ha_lbl_* (slug) string changed; no OTmap[] / OTGW-Core.h modification
- [x] #6 OTDirect_* and SAT_BLE_* strings unchanged (already correctly underscored)
- [ ] #7 Field validation: Andre confirms HA on OTGW32 shows clean Title-Case friendly names matching dev branch output
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. apply.py mapping; 2. verify scope (no labels/OTmap/slugs touched, OTDirect+SAT_BLE preserved); 3. build firmware+filesystem; 4. evaluate --quick; 5. bump prerelease; 6. stage + commit + push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of dev TASK-573 to the 2.0.0 ESP32/SAT feature line. Same 125 defective HA-discovery friendly-name PROGMEM strings normalised, this time in `src/OTGW-firmware/MQTTHaDiscovery.cpp` (separate translation unit per ADR-077, vs dev's `mqtt_configuratie.cpp`). Mapping is byte-identical to dev's so HA renders the same friendly names on both firmware lines.

Scope (intentionally minimal):
- 125 shared `ha_name_*` PROGMEM string values rewritten via `/tmp/friendly-rename/apply.py` against `/tmp/friendly-rename/dev.tsv`.
- Duplicate variable `ha_name_chpumpoperationhoursg` declaration removed; its sole caller retargeted to `ha_name_chpumpoperationhours`.
- `ha_lbl_*` slug labels, `OTmap[]` descriptions, msgid table flags, and topic slugs are untouched (Test 1 added-label-line count = 0).
- 2.0.0-only `OTDirect_Flame_*` and `SAT_BLE_*` strings preserved unchanged (diff shows no OTDirect/SAT_BLE lines touched).

Verification evidence:
- `git diff --stat`: 126 insertions / 128 deletions on `MQTTHaDiscovery.cpp` (matches dev's commit shape).
- Build green: `./build.sh` succeeded for both ESP32-S3 and ESP8266 targets, producing merged-full + littlefs binaries.
- Evaluator green: `python3 evaluate.py --quick` → 59 passed / 2 warnings (pre-existing) / 0 failed; health 97.1%.
- Prerelease bumped `alpha.19 → alpha.20` via `bin/bump-prerelease.sh` (44 file-banner version strings + version.h + data/version.hash).

AC #7 (field validation: Andre confirms HA on OTGW32 shows clean Title-Case friendly names matching dev branch) remains open pending Andre's reflash to alpha.20 — same gating as dev TASK-573.
<!-- SECTION:FINAL_SUMMARY:END -->
