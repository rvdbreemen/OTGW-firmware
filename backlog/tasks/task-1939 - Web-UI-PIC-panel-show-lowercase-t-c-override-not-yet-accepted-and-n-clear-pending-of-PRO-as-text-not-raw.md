---
id: TASK-1939
title: >-
  Web UI PIC panel: show lowercase t/c (override not yet accepted) and n (clear
  pending) of PR=O as text, not raw
status: To Do
assignee: []
created_date: '2026-11-14T15:51:00Z'
labels:
  - webui
  - parity
dependencies: []
priority: low
ordinal: 993000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1848 (95, 2026-11-14). fmtPicSetting() in src/OTGW-firmware/data/v2.js:6300-6304 knows only N/0, T<temp> and C<temp> for setpoint_override. The PIC (PrintOverride, gateway.asm 6.8:2201-2226) prints lowercase t/c while the thermostat has not yet accepted the TT=/TC= override and n while a clear is still pending; the web UI then shows the raw value (e.g. 't20.50'). Already so on PIC boards today; OT-Direct gets the same letters with TASK-1848. Web UI work for 09's area.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 t<temp> and c<temp> render as the temporary/constant override text with a 'not yet accepted by the thermostat' note
- [ ] #2 n renders as 'override being cleared'
- [ ] #3 host webui test red on the current v2.js, green on the fix; i18n keys in every language pack
<!-- AC:END -->
