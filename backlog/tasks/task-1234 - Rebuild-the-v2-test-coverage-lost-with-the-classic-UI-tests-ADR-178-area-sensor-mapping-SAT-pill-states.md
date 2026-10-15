---
id: TASK-1234
title: >-
  Rebuild the v2 test coverage lost with the classic UI tests (ADR-178
  area-sensor mapping, SAT pill states)
status: To Do
assignee: []
created_date: '2026-10-15 10:41'
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
- [ ] #1 A node test exercises v2.js's area-sensor mapping for Dallas addresses and BLE MACs, and ADR-178's guard reference points at it
- [ ] #2 sat-status-pill covers the safety, window, summer, disabled and missing-boiler_status states against v2.js
- [ ] #3 Both tests fail against a deliberately broken copy and pass against v2.js (old-vs-fix)
<!-- AC:END -->
