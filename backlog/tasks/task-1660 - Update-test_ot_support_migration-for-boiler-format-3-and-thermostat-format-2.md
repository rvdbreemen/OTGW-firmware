---
id: TASK-1660
title: Update test_ot_support_migration for boiler format 3 and thermostat format 2
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T17:37:00Z'
updated_date: '2026-11-06T01:01:00Z'
labels:
  - tests
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 784000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
test/host/test_ot_support_migration.py fails M2, M3, C2 on dev: stale expectations since 8442aaa1c (TASK-1391, /ot-boiler.json v3) and b8b174bce (TASK-1612, /ot-thermo.json v2); not a firmware bug (read-only diagnosis 2026-11-05). Fix: M2 expects v3 header; M3 fixture v3 + new discard case for v2; C2 fixture v2 + discard case for v1; refresh MUTANTS MX2/MX3 anchors and the CONTROL/DEFECT old-rev data.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 test_ot_support_migration.py RESULT: PASS on dev
- [x] #2 New discard cases: a v2 boiler file and a v1 thermostat file are not loaded and are rewritten
- [x] #3 Mutants MX2/MX3 match their anchors again (no harness error 2)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 02:40: fixture updated (no firmware change). M2 expects boiler format 3; M3 loads a format-3 file; new M4 (format-2 boiler file: not loaded, dirty, rewritten as format 3 with empty arrays), M5 (format-1 thermostat file: not loaded, dirty, rewritten as format 2 with empty arrays), M6 (format-2 thermostat file loads); C2 is now the thermostat save/reload round trip. OLD rev moved to 8442aaa1c^ (boiler 2, thermostat 1). Mutants MX1-MX3 re-anchored, MX4-MX6 added.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: python test/host/test_ot_support_migration.py on dev (working tree) prints RESULT: PASS (M1-M6, C1-C4 pass, 0 failures). AC2: new cases M4 (format-2 boiler file not loaded and rewritten as format 3) and M5 (format-1 thermostat file not loaded and rewritten as format 2) pass on FIX and fail on OLD 8442aaa1c^. AC3: --old-rev 8442aaa1c^ RESULT: PASS; every mutant pattern matched once (no exit 2) and MX1 fails M1,M2,M4; MX2 C1,M2,M4; MX3 M1,M2; MX4 M4; MX5 M5; MX6 C2,M5, each exactly as listed; OLD fails exactly M2-M6 with the listed differing parts, controls M1,C1-C4 pass on OLD. Test-only change, no firmware path, no bump or build needed.
<!-- SECTION:FINAL_SUMMARY:END -->
