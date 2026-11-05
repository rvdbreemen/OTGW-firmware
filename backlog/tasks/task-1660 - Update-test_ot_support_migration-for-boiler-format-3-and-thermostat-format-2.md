---
id: TASK-1660
title: Update test_ot_support_migration for boiler format 3 and thermostat format 2
status: To Do
assignee: []
created_date: '2026-11-05T17:37:00Z'
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
- [ ] #1 test_ot_support_migration.py RESULT: PASS on dev
- [ ] #2 New discard cases: a v2 boiler file and a v1 thermostat file are not loaded and are rewritten
- [ ] #3 Mutants MX2/MX3 match their anchors again (no harness error 2)
<!-- AC:END -->
