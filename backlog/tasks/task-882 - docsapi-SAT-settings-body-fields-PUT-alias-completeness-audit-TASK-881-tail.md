---
id: TASK-882
title: >-
  docs(api): SAT settings body fields + PUT-alias completeness audit (TASK-881
  tail)
status: To Do
assignee: []
created_date: '2026-06-18 04:31'
labels: []
dependencies: []
ordinal: 98000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Source-dive remainder split off from TASK-881. Two openapi.yaml completeness items that need handler/source verification (no GET to field-diff against): (1) SAT settings request bodies (/v2/sat/settings, /v2/sat/settings/{name}) may be missing fields documented by satParseSettings in SATcontrol.ino (audit flagged flame_duty_pct, hysteresis/vent). /v2/sat/settings GET returns 405 (POST/PUT only), so the body schema must be derived from source. (2) PUT-alias completeness: confirm which POST handlers also accept PUT and document the alias on each. All 8 HIGH + high-value medium/low (basicAuth on 40 mutations, pic 503s, device/info cap, anyOf) already shipped under TASK-881 (commits 99fb675f, dbbf6fa4, bd6696d4).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SAT settings request-body schemas match satParseSettings field set in SATcontrol.ino
- [ ] #2 Every POST handler that also accepts PUT documents the PUT alias
- [ ] #3 openapi.yaml validates
<!-- AC:END -->
