---
id: TASK-1777
title: >-
  Web UI Greek review: apply Sergeant D's corrections to el.json (first: the DHW
  switch on the Hot water card)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T12:18:00Z'
updated_date: '2026-11-10T15:05:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 874000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D (Greek native speaker) reviews the Greek web UI pack. First correction, #dev-sat-mqtt 2026-11-10 09:36:08 UTC msg 1569631735581253643, verbatim: 'The domestic hot water enable switch in the Hot water card should be remained to “Ενεργοποίηση ΖΝΧ”'. Ambiguous: h_mon.sat_dhw_standalone ('Αυτόνομος έλεγχος ΖΝΧ', always visible) or h_mon.sat_dhw_enable ('Ενεργοποίηση ΖΝΧ (HW=)', tank only). Asked A/B (msg 1569635443606159451); 09:53 he asked for time until he is near his board. More corrections may follow; collect them here. Also review the LCD abbreviations ΘΕΡΜ/ΑΘ (TASK-1775 choice).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each correction recorded verbatim with msg link in docs/sat/sergeant-d-corrections.md before it is applied
- [ ] #2 el.json changed only for keys he named; i18n-lang and i18n-followups tests green
- [ ] #3 Shipped in an alpha build and checked on a bench unit in Greek
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 13:50 Sergeant D answered (entry 292): A (the always-visible switch) -> 'Ενεργοποίηση ΖΝΧ', SAT Hot water title -> 'Ζεστό Νερό Χρήσης', Home grid cell home.cell_room 'ΧΩΡΟΣ' -> 'ΕΣΩΤΕΡΙΚΗ'. Recorded in docs/sat/sergeant-d-corrections.md entry 292 (index refreshed) before applying. el.json: h_mon.sat_hw_title, h_mon.sat_dhw_standalone (also its aria label, same key), home.cell_room. Tests: i18n-lang 6/6, i18n-followups 3/3, i18n-no-hardcoded 3/3. Open questions via 97: the slider hint wording, the near-identical tank switch label 'Ενεργοποίηση ΖΝΧ (HW=)', and whether other 'Ζεστό νερό'/'Χώρος' keys follow.

2026-11-10 14:15 beta.170 posted (msg 1569700928221749350, 2dda513da; bench .61 on the identical tree 749ba83: live el title 'Ζεστό Νερό Χρήσης', switch + aria 'Ενεργοποίηση ΖΝΧ'; 'ΕΣΩΤΕΡΙΚΗ' in the served /i18n/el.json, #cGrid hidden on .61 so the cell is not seen live). Q1 answered B (entry 293): h_mon.sat_dhw_hint now names the switch; committed without its own build, ships with the next build together with the Q2/Q3 answers.

2026-11-10 14:40 entry 294: Q2 answer A (tank switch label stays); slider label h_mon.sat_dhw_setpoint -> 'Θερμοκρασία στόχος ΖΝΧ' (verbatim, slider only; settings.label.satdhwsetpoint waits for Q3). His 'keep only one hot-water card' is a layout change for Robert (coordinator files it). Committed, no build yet.

2026-11-10 14:55 found while checking Q3: the Home chips (v2.js:2937) and the Home cell grid (renderCGrid, v2.js:3061) showed a hard-coded English 'DHW'. Now t('home.chip_dhw') / t('home.cell_dhw'), nl/de 'WW', el 'ΖΝΧ' (glossary). i18n-followups test 4: red on the previous v2.js, green on the fix. Fit check for Q3 (local render, 1036 and 390 px): 'Ζεστό Νερό Χρήσης' fits the Home tile on one line (173 px tile), 'ΕΣΩΤΕΡΙΚΗ' fits the Home drawing label (ends x=745 of 800).

2026-11-10 16:00 entry 295: Q3 answered A ('Α'): el.json h_home.tile_dhw, h_home.stat_dhw, settings.group.hot_water -> 'Ζεστό Νερό Χρήσης'; h_home.svg_room -> 'ΕΣΩΤΕΡΙΚΗ'; settings.label.satdhwsetpoint -> 'Θερμοκρασία στόχος ΖΝΧ'. Scope = the Q3 list; adv.pic_led_w and the mixed-case 'Χώρος' keys unchanged. All Greek review answers (Q1-Q3) are in; build after the 16:21 public-beta freeze and the 4c/detectie benches, re-bumped to the free number.
<!-- SECTION:NOTES:END -->
