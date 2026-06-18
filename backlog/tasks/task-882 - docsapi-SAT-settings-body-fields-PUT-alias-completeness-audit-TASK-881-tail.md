---
id: TASK-882
title: >-
  docs(api): SAT settings body fields + PUT-alias completeness audit (TASK-881
  tail)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-18 04:31'
updated_date: '2026-06-18 08:13'
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
- [x] #1 SAT settings request-body schemas match satParseSettings field set in SATcontrol.ino
- [x] #2 Every POST handler that also accepts PUT documents the PUT alias
- [x] #3 openapi.yaml validates
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-867 ArduinoJson migration changed REST types (string-bools -> real bools, dtostrf-quoted numbers -> native JSON numbers) across many endpoints. Done in 867: /v2/health schema (3 bools string->boolean) + the x-api-design-notes boolean section. REMAINING for this task: full OpenAPI re-sync against the post-migration golden (scripts/json-golden/, re-captured alpha.210+) - audit every endpoint's documented field types vs the now-corrected live output (device/info, settings, otmonitor/telegraf, sat/*, otdirect/*). Use scripts/json_golden.py captures as the source of truth.

MIGRATION DOC-SYNC PORTION DONE (committed a55a87c6): the only explicit response-type bug from the ArduinoJson migration was /v2/health (3 bools documented as type:string enum[true,false]) - fixed to type:boolean + the x-api-design-notes 'booleans as strings' note corrected. Systematic re-scan found NO other 'enum:[true,false]' string-bool patterns and verified the other suspect fields (otdirectavailable, crashlog.available, simulate.active) are already correctly typed boolean. roomcomp/kp at openapi 1958 are POST /v2/otdirect/settings REQUEST-body fields (the API accepts string values), not response contracts, so not migration mismatches. REMAINING (original 882 scope, source-dive, NOT migration-driven): SAT settings request-body field completeness (flame_duty_pct, hysteresis/vent vs satParseSettings) + PUT-alias documentation. Set back To Do - this is separate doc-completeness work, no longer blocked on the migration.

RESOLVED. (1) SAT settings body fields: PHANTOM audit item - flame_duty_pct/hysteresis-as-setting/vent do NOT exist in source. /v2/sat/settings/{name} is a GENERIC name/value command dispatcher (restAPI.ino:1201, mirrors MQTT sat/<name> sub-commands like heating_curve/deadband/reset_integral), already correctly documented in openapi as a {name} path param + plain body - there is no fixed-field body to complete. (2) PUT aliases: the firmware gates all v2 mutations on (HTTP_POST || HTTP_PUT), so PUT is a transparent alias on EVERY post: endpoint. Documented this globally in x-api-design-notes (HTTP Methods section) rather than duplicating 20+ put: blocks (KISS). YAML validates.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Original audit premise was largely invalid: the 'missing SAT settings fields' (flame_duty_pct/hysteresis/vent) are phantom - they don't exist in firmware; /v2/sat/settings/{name} is a generic name/value dispatcher already correctly documented. The real item - PUT aliases - is systematic (all POST mutations also accept PUT), now documented once globally in x-api-design-notes instead of per-endpoint duplication. Plus the migration response-type sync (health string-bools->boolean) shipped under TASK-867. openapi.yaml validates.
<!-- SECTION:FINAL_SUMMARY:END -->
