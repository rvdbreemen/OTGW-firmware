---
id: TASK-1352
title: 'SAT: last cycle class never expires (thermo-nova drops it after 6 h)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T16:28:00Z'
updated_date: '2026-10-24T16:43:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 487000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #55: SATcycles.ino:588 keeps the last cycle class forever; thermo-nova cycles/history.py:48-57 drops it after 6 h, so HA keeps showing an old cycle problem. Display/state only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Last cycle class reported as none after 6 h without a new cycle (host test red on HEAD, green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: readers gate: reporting (SATcontrol 2591/2989/3227, restAPI 1002/1012/2387, debug) + satUpdateFlameStatus (only feeds sat/flame_status/health publishers) now use satReportedLastCycleClass(); satAutoTuneUpdate (SATcontrol 4623-4625) keeps the raw class (read in the pass that sees the new cycle, always fresh) - no control change. thermo-nova history.py:48-57 + cycles/const.py:14 (6*3600, '>'). tests/test_sat_last_cycle_expiry.py 10/10 OK on fix; HEAD SATtypes.h: helper missing -> FAILED. pwm_override 70 OK, mqtt_state 13 OK, evaluate 0 failed, build.bat SUCCESS 18:42. Follow-up: stall threshold uses fLastCycleDuration that never expires (SATcontrol.ino:678 vs thermo-nova device/status.py:135-162 falls back to 600 s) - may reach control, separate task for Sergeant D.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The last cycle class is reported as none once the last cycle ended more than 6 h ago, as thermo-nova (cycles/history.py:48-57, const.py:14): REST, MQTT sat/cycle_class and cycle_health, WS snapshot, flame status, debug. Auto-tune keeps the raw value (always fresh there). Evidence: host test 10/10, red on HEAD; build SUCCESS. Not verified on a device after 6 h.
<!-- SECTION:FINAL_SUMMARY:END -->
