---
id: TASK-3056
title: >-
  SAT: bound the %s of the BLE pin notice narration so test_sat_narrate_fits is
  green (SATble.ino:851, TASK-3011)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-25T11:36:00Z'
updated_date: '2026-11-25T13:00:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tests/test_sat_narrate_fits.py fails on origin/dev: SATble.ino:851 satNarratef_P("SAT uses BLE sensor %s: confirm or change it on the SAT page", settings.sat.sBleMAC) has a %s without a bound in KNOWN_S_WIDTHS (TASK-3011, 5076410fd). sBleMAC is char[18] (SATtypes.h:651), so the argument is at most 17 characters and the line is about 76 characters against SAT_NARRATE_BUF 128, but nothing in code or test says so. Bound the argument in the code (%.17s) and register the width 17 in the test's KNOWN_S_WIDTHS so the narration provably fits. Small src commit on a fresh lane from origin/dev after otgw-webui's push, with its own bump. Found in the beta.429 full-tests rerun.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host test is red on origin/dev (test_sat_narrate_fits, the SATble.ino:851 line) and green on the fix; the %s in the code is bounded
- [x] #2 evaluate.py --quick no new FAIL; esp32-combo build OK
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 fix: SATble.ino:851 prints sBleMAC (char[18], SATtypes.h:651) with %.17s; width 17 registered in tests/test_sat_narrate_fits.py KNOWN_S_WIDTHS under the bounded format text; the pin in tests/test_sat_ble_pin_only.py:329 follows. Red/green: test_sat_narrate_fits RED on origin/dev 27105193c ('SATble.ino:851 has a %s without a bound'), GREEN on the fix; mutant (code back to unbounded %s, registry kept) is RED again, so the bounded form is enforced. Also green: test_sat_ble_pin_only 21, tests/webui/sat-ble-pin-room-source 5. evaluate --quick 89 passed / 0 failed. Build and bench pending; no behaviour change (output identical for a 17-character MAC).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Change: SATble.ino:851 printed the pinned BLE MAC (settings.sat.sBleMAC, char[18], SATtypes.h:651) with an unbounded %s in the SAT narration; test_sat_narrate_fits rejected it (TASK-3011). Now %.17s, and width 17 is registered in KNOWN_S_WIDTHS (tests/test_sat_narrate_fits.py) under the bounded format text; the pin in tests/test_sat_ble_pin_only.py:329 follows. Output is identical for a 17-character MAC: no behaviour change. Evidence per AC: AC1 test_sat_narrate_fits RED on origin/dev 27105193c ('SATble.ino:851 has a %s without a bound'), GREEN on the fix, RED again on the mutant (code back to unbounded %s, registry kept), so the bounded form is enforced; test_sat_ble_pin_only 21 OK, tests/webui/sat-ble-pin-room-source 5 pass. AC2 evaluate --quick 89 passed / 0 failed; esp32-combo build from 43ac963e3 'Build completed successfully!', app 2158544 B of 2293760 B (94.1 %), free 135216 B; pushed FF baed33eca..43ac963e3, zip posted to #alpha-testing (message 1575134057333919997). Bench (otgw-bench-tester, .63, beta.432+43ac963, OTGW-field-data/bench-63/2026-11-25_narrate-432/): OTA +2 (390 -> 392), githash 43ac963, PIC 6.8, crashlog 404, settings byte-identical, the line 'SAT uses BLE sensor <full 17-character MAC>: confirm or change it on the SAT page' exactly once per boot on boot 391 (13:43:46.400) and boot 393 (13:48:14.030, deliberate reboot), not truncated, 10 min stable, hd_rest_503 0. NOT VERIFIED: boot 392 was not captured (the logger connected 0.5 s after the first BLE window), so the once-per-boot claim rests on boots 391 and 393 plus the code (s_pinNoticeLogged, SATble.ino:848-853); that boot is undecided.
<!-- SECTION:FINAL_SUMMARY:END -->
