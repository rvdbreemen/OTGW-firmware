---
id: TASK-3056
title: >-
  SAT: bound the %s of the BLE pin notice narration so test_sat_narrate_fits is
  green (SATble.ino:851, TASK-3011)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T11:36:00Z'
updated_date: '2026-11-25T12:36:00Z'
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
- [ ] #2 evaluate.py --quick no new FAIL; esp32-combo build OK
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 fix: SATble.ino:851 prints sBleMAC (char[18], SATtypes.h:651) with %.17s; width 17 registered in tests/test_sat_narrate_fits.py KNOWN_S_WIDTHS under the bounded format text; the pin in tests/test_sat_ble_pin_only.py:329 follows. Red/green: test_sat_narrate_fits RED on origin/dev 27105193c ('SATble.ino:851 has a %s without a bound'), GREEN on the fix; mutant (code back to unbounded %s, registry kept) is RED again, so the bounded form is enforced. Also green: test_sat_ble_pin_only 21, tests/webui/sat-ble-pin-room-source 5. evaluate --quick 89 passed / 0 failed. Build and bench pending; no behaviour change (output identical for a 17-character MAC).
<!-- SECTION:NOTES:END -->
