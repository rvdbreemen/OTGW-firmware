---
id: TASK-1939
title: >-
  Web UI PIC panel: show lowercase t/c (override not yet accepted) and n (clear
  pending) of PR=O as text, not raw
status: To Do
assignee: []
created_date: '2026-11-14T15:51:00Z'
updated_date: '2026-11-14T17:27:00Z'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (95) prepared, NOT applied and NOT run (commit charge 95 %, and a src/ change in wt-95w would block the beta.252-254 post): patch C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\87fc885c-0e3d-40a7-9313-4a23af8d2bc3\scratchpad\t1939\t1939.patch (fmtPicSetting in v2.js: n -> adv.pic_ovr_clearing; t/c -> the temp/const text wrapped in adv.pic_ovr_pending; new keys in en (built in), nl, de, el; JSON validated) and a draft headless-Chrome test pic-setpoint-override-text.test.mjs next to it (6 cases N, n, T, C, t, c via a fake /api/v2/pic/settings). Next: apply after the 1848/1849/1940 push, run the test red on the current v2.js and green on the patch, one file at a time.
<!-- SECTION:NOTES:END -->
