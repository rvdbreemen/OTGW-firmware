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
