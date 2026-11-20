---
id: TASK-2012
title: >-
  Capture device-state sweep: count a 503 for absent hardware (OT-Direct routes
  on a PIC board, PIC routes on OT-Direct) as not applicable, not failed
status: Done
assignee:
  - '@claude'
created_date: '2026-11-18T05:56:00Z'
updated_date: '2026-11-18T06:14:00Z'
labels:
  - tools
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1031000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1544 AC3 (bench .63, beta.360 tools): the device-state summary line read '36 answered / 5 not on this firmware (404) / 3 failed'; the 3 failed are /api/v2/otdirect/status, /settings and /overrides, which answer 503 'No OT-Direct hardware: OT-Direct functions disabled' on a PIC board by design (restAPI.ino:542). The same holds for the /api/v2/pic/* routes on an OT-Direct board ('No PIC detected - PIC functions disabled', restAPI.ino:528). Tools only (scripts/otgw_capture.py sweep_device), no src, no bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sweep_device counts a 503 whose body says the hardware is absent as 'not on this board', in its own count in the summary line; a host test is red on the old code and green on the fix
- [x] #2 Any other 503 (queue full, busy) still counts as failed; covered by the same test
- [x] #3 scripts/README.md device-state paragraph names the new count
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-18 red/green (python -m unittest discover -s tests -p test_capture_device_data.py -v): fix 20/20 OK; old scripts/otgw_capture.py (git show HEAD) FAILED (failures=2): test_absent_hardware_503_is_not_applicable got '27 answered, 13 not on this firmware (404), 4 failed', test_other_503_still_counts_as_failed got '29 answered, 13 ... 2 failed'. The second test is red on old only through the new count's absence; its '2 failed' part held on old too, so it is a no-regression check for AC2, not a reproduction. Strings checked: restAPI.ino:528 and :759 'No PIC detected - PIC functions disabled', :542 'No OT-Direct hardware: OT-Direct functions disabled'. Tools only, no src, no bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: sweep_device counts a 503 with an absent-hardware body as 'not on this board (503)'; test red on old (4 failed), green on fix. AC2: other 503 (queue full, busy) stays failed; same test file, no-regression (2 failed on old and fix). AC3: scripts/README.md device-state paragraph names the four counts. Not verified on a live unit: next capture run on .63 shows the new line.
<!-- SECTION:FINAL_SUMMARY:END -->
