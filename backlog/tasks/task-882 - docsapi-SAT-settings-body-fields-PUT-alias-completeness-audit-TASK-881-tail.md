---
id: TASK-882
title: >-
  docs(api): SAT settings body fields + PUT-alias completeness audit (TASK-881
  tail)
status: To Do
assignee: []
created_date: '2026-06-18 04:31'
updated_date: '2026-06-18 06:54'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-867 ArduinoJson migration changed REST types (string-bools -> real bools, dtostrf-quoted numbers -> native JSON numbers) across many endpoints. Done in 867: /v2/health schema (3 bools string->boolean) + the x-api-design-notes boolean section. REMAINING for this task: full OpenAPI re-sync against the post-migration golden (scripts/json-golden/, re-captured alpha.210+) - audit every endpoint's documented field types vs the now-corrected live output (device/info, settings, otmonitor/telegraf, sat/*, otdirect/*). Use scripts/json_golden.py captures as the source of truth.
<!-- SECTION:NOTES:END -->
