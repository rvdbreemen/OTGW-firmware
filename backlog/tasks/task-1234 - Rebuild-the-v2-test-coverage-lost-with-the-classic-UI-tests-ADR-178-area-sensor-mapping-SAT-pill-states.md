---
id: TASK-1234
title: >-
  Rebuild the v2 test coverage lost with the classic UI tests (ADR-178
  area-sensor mapping, SAT pill states)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 10:41'
updated_date: '2026-10-15 13:25'
labels:
  - test
  - webui
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 372000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decided 2026-10-15: the classic-UI tests were deleted with the classic UI (TASK-1223.12, ADR-192). Two pieces of coverage still matter for v2: (1) ADR-178 names tests/webui/sat-area-sensor-mapping.test.mjs as its guard (area-sensor reference: Dallas address or BLE MAC); (2) sat-status-pill lost the safety, window, summer, disabled and missing-boiler_status cases, which ran against classic only. The old tests are in git at 3aa1ef927.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A node test exercises v2.js's area-sensor mapping for Dallas addresses and BLE MACs, and ADR-178's guard reference points at it
- [x] #2 sat-status-pill covers the safety, window, summer, disabled and missing-boiler_status states against v2.js
- [x] #3 Both tests fail against a deliberately broken copy and pass against v2.js (old-vs-fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-15:
- tests/webui/sat-area-sensor-mapping.test.mjs (new, 13 checks) and sat-status-pill.test.mjs (extended, 14 checks) run the real v2.html/v2.js in headless Chrome against a fake device server.
- Old-vs-fix: 11 mutations of temp copies (7 pill, 4 area) each make the run FAIL with exit 1; the real v2.js passes.
- The ADR-178 verification reference now points at the new test. ADR-178 is Proposed, decision text unchanged, lint passes strictly.
- Not covered by a UI test: the firmware side of ADR-178 (sSensorArea width is guarded by its own enforcement regex).
- Observation for the maintainer: during a safety trip with the burner firing, the v2 pill still reads 'Heating' while the headline says 'Safety tripped!'. The test pins the current behaviour.
Re-run after the concurrent v2.js edits (TASK-1226..1233) before commit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Rebuilt the v2 coverage lost with the classic tests: tests/webui/sat-area-sensor-mapping.test.mjs (13 checks, ADR-178 guard reference updated) and 5 new state cases in sat-status-pill.test.mjs (14 checks), both against the real v2.html/v2.js in headless Chrome. Old-vs-fix: 11 mutations each fail, real code passes; rerun green after the TASK-1226..1233 v2.js edits.
<!-- SECTION:FINAL_SUMMARY:END -->
