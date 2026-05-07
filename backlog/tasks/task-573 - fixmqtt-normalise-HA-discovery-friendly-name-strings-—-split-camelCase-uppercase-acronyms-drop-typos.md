---
id: TASK-573
title: >-
  fix(mqtt): normalise HA discovery friendly-name strings — split camelCase,
  uppercase acronyms, drop typos
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 21:23'
updated_date: '2026-05-07 21:27'
labels:
  - mqtt
  - ha-discovery
  - friendly-name
  - andre-feedback
  - polish
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Andre's field reports surfaced systematic friendly-name rendering defects after the writeFriendlyName helper landed (beta.27/alpha.18). The helper splits on '_' only, so ~125 friendly-name PROGMEM strings need underscores inserted, lowercase acronyms uppercased (Memberid -> MemberID, vh -> VH, dhw -> DHW, rbp -> RBP, ch2 -> CH2), camelCase glue split (ElectricalCurrentBurnerFlame -> Electrical_Current_Burner_Flame), and a stray typo'd variable removed (ha_name_chpumpoperationhoursg -> retarget caller to ha_name_chpumpoperationhours and delete duplicate). Mapping table generated from /tmp/friendly-rename/dev.tsv; same mapping applies byte-identical on 2.0.0 worktree (TASK-571 there). Touches ONLY friendly-name PROGMEM string values + one duplicate variable removal — labels (ha_lbl_*), msgid table structure, OTmap descriptions, and slug topics are out of scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All 125 friendly-name strings in mqtt_configuratie.cpp normalised per /tmp/friendly-rename/dev.tsv mapping
- [x] #2 Typo'd variable ha_name_chpumpoperationhoursg removed; line 1009 caller retargeted to ha_name_chpumpoperationhours
- [x] #3 Build green: python build.py --firmware exits 0
- [x] #4 Evaluator green: python evaluate.py --quick shows no new failures
- [x] #5 No ha_lbl_* (slug) string changed — git diff confirms zero label-line modifications
- [x] #6 No OTmap[] / OTGW-Core.h modification — git diff confirms zero touches outside mqtt_configuratie.cpp
- [ ] #7 Field validation: Andre confirms HA shows clean Title-Case friendly names (Memberid -> MemberID, no glued strings, no trailing g on CH Pump Operation Hours)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. apply.py mapping; 2. verify scope (no labels/OTmap/slugs touched); 3. build firmware+filesystem; 4. evaluate --quick; 5. bump prerelease; 6. stage + commit + push
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Normalised HA discovery friendly-name PROGMEM strings in `mqtt_configuratie.cpp` per the dev.tsv mapping (126 entries) so the writeFriendlyName helper that landed in beta.27 produces clean Title-Case labels in Home Assistant.

## Scope

ONLY `ha_name_*` PROGMEM string values + one duplicate-variable removal. Out of scope and untouched: `ha_lbl_*` labels, `OTmap[]` descriptions, msgid table flags, slug topics, `OTGW-Core.h`.

## What changed

- ~125 friendly-name strings normalised in a single sweep via `apply.py`:
  - camelCase / glued tokens split (e.g. `ElectricalCurrentBurnerFlame` -> `Electrical_Current_Burner_Flame`, `OEMFaultCode` -> `OEM_Fault_Code`, `SolarStorageASFflags` -> `Solar_Storage_ASF_Flags`).
  - Lowercase acronym fragments uppercased (`Memberid` -> `MemberID`, `vh_*` -> `VH_*`, `dhw_*` -> `DHW_*`, `rbp_*` -> `RBP_*`, `ch2` -> `CH2`).
  - Stray typos fixed (`CHPumpOperationHoursg` -> `CH_Pump_Operation_Hours`).
- Duplicate variable `ha_name_chpumpoperationhoursg` declaration removed; its sole reference (msgid 121 row) retargeted to the clean sibling `ha_name_chpumpoperationhours`.
- Compound product names preserved intact (`DayTime`, `OTDirect`).

## Verification

- `git diff --stat`: 126 insertions / 128 deletions (= 126 string updates + 1 decl line + 1 retargeted reference). Matches mapping size exactly.
- Scope-check greps: zero `+const char ha_lbl_` lines, zero `+const char ha_name_chpumpoperationhoursg` lines, zero leaked tokens (`Memberid`/`CHPumpOperationHoursg`/`ElectricalCurrentBurnerFlame`/`Dayofweek`) inside `ha_name_*` strings. The single residual `ElectricalCurrentBurnerFlame` is in `ha_lbl_electricalcurrentburnerflame` (label, intentionally out of scope).
- `./build.sh`: exit 0; firmware 0.70 MB, filesystem 1.98 MB.
- `python3 evaluate.py --quick`: 31 passed / 2 warnings / 1 fail (all pre-existing baseline — header guard, JSON buffer arithmetic heuristic, and 2 unresolved ADR refs out of 1143). No new failures from this change; string-only diffs cannot affect those checks.

## Release

Prerelease bumped beta.28 -> beta.29 via `bin/bump-prerelease.sh`.

## Field validation (AC #7 — gated)

AC #7 stays unchecked: requires Andre's reflash + visual confirmation in his HA instance that the rendered friendly names are clean Title-Case (no `Memberid` literal, no glued strings, no trailing `g` on CH Pump Operation Hours).
<!-- SECTION:FINAL_SUMMARY:END -->
