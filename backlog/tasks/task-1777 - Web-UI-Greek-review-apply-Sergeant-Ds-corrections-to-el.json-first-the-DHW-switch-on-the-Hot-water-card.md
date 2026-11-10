---
id: TASK-1777
title: >-
  Web UI Greek review: apply Sergeant D's corrections to el.json (first: the DHW
  switch on the Hot water card)
status: To Do
assignee: []
created_date: '2026-11-10T12:18:00Z'
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
- [ ] #1 Each correction recorded verbatim with msg link in docs/sat/sergeant-d-corrections.md before it is applied
- [ ] #2 el.json changed only for keys he named; i18n-lang and i18n-followups tests green
- [ ] #3 Shipped in an alpha build and checked on a bench unit in Greek
<!-- AC:END -->
