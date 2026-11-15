---
id: TASK-1939
title: >-
  Web UI PIC panel: show lowercase t/c (override not yet accepted) and n (clear
  pending) of PR=O as text, not raw
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T15:51:00Z'
updated_date: '2026-11-15T10:38:00Z'
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
- [x] #1 t<temp> and c<temp> render as the temporary/constant override text with a 'not yet accepted by the thermostat' note
- [x] #2 n renders as 'override being cleared'
- [x] #3 host webui test red on the current v2.js, green on the fix; i18n keys in every language pack
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (95) prepared, NOT applied and NOT run (commit charge 95 %, and a src/ change in wt-95w would block the beta.252-254 post): patch C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\87fc885c-0e3d-40a7-9313-4a23af8d2bc3\scratchpad\t1939\t1939.patch (fmtPicSetting in v2.js: n -> adv.pic_ovr_clearing; t/c -> the temp/const text wrapped in adv.pic_ovr_pending; new keys in en (built in), nl, de, el; JSON validated) and a draft headless-Chrome test pic-setpoint-override-text.test.mjs next to it (6 cases N, n, T, C, t, c via a fake /api/v2/pic/settings). Next: apply after the 1848/1849/1940 push, run the test red on the current v2.js and green on the patch, one file at a time.

2026-11-15: PIC source checked in this session: other-projects/otgw-6.6/gateway.asm:2198-2222 PrintOverride (the task cites 6.8:2201-2226; 6.8 is not in the repo): N = no override flags, n = flags set but setpoint 0 ('Waiting to clear the override'), T/C with iorlw 0x20 (lowercase) while OverrideSet is clear ('Override accepted by the thermostat?'). fmtPicSetting() renders t/c as adv.pic_ovr_temp_pending/const_pending and n as adv.pic_ovr_clearing; en, nl, de, el. Evidence: tests/webui/pic-setpoint-override-text.test.mjs (real fmtPicSetting): 4 pass on the fix; on the old v2.js 3 fail (t/c, n, keys) and 1 pass (T/C/N unchanged). i18n-followups, i18n-lang, i18n-no-hardcoded, values-names-follow-language pass. The Greek strings are a first translation (George reviews, TASK-1724).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: t<temp>/c<temp> render as the temporary/constant override text with '(not yet accepted by the thermostat)'. AC2: n renders as 'Override being cleared'. AC3: tests/webui/pic-setpoint-override-text.test.mjs on the real fmtPicSetting(): 4 pass on the fix, 3 fail on the old v2.js; the three keys are in v2.js (en) and in i18n/nl, de, el (Greek: first translation, George reviews). Not checked on a PIC unit with a pending override (the value shapes come from gateway.asm 6.6 PrintOverride).
<!-- SECTION:FINAL_SUMMARY:END -->
