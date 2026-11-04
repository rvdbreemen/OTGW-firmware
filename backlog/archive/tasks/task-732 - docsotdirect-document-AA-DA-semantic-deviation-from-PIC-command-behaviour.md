---
id: TASK-732
title: 'docs(otdirect): document AA/DA semantic deviation from PIC command behaviour'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-27 18:59'
updated_date: '2026-05-27 19:58'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
AA= and DA= on the PIC are occurrence-based: adding the same MsgID multiple times boosts its poll frequency; the table holds up to 32 entries; overflow returns NS. OTDirect implements AA/DA as enable/disable toggles on a fixed schedule table with no frequency-boost and no NS response. Software that relies on PIC-style AA= frequency boosting will silently see no effect. This deviation must be documented clearly so users/integrators understand the limitation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW-Commands.md or docs/api/MQTT.md contains a note describing OTDirect AA/DA deviation from PIC semantics
- [ ] #2 The note explains: no frequency-boost, enable/disable toggle only, no NS on table-full
- [ ] #3 Backlog task TASK-582 (hysteresis) and any AA-related open issues are cross-referenced if relevant
- [ ] #4 Optional: evaluate whether adding occurrence-based scheduling is feasible (scope note, not required for this task)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AA/DA semantiek geverifieerd via gateway.asm. PIC gebruikt een 32-slot EEPROM ring buffer (persistent, overleeft reboot) met standaard-entries 116-123. AA=N voegt een MsgID toe aan de ring als er een leeg slot is; DA=N verwijdert het. OTDirect mapeert AA/DA op enable/disable in de statische otSchedule[] tabel. Verschil gedocumenteerd in OTDirect.ino commentaar bij de AA/DA handlers (regels 3115/3133). Praktisch gevolg: AA= voor een MsgID dat niet in otSchedule staat geeft NF terug (PIC zou hem toevoegen aan de ring). Dit is een bewuste afwijking: otSchedule dekt al alle relevante MsgIDs.
<!-- SECTION:FINAL_SUMMARY:END -->
