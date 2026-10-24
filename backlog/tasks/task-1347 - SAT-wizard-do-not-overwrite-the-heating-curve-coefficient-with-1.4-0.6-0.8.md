---
id: TASK-1347
title: 'SAT wizard: do not overwrite the heating curve coefficient with 1.4/0.6/0.8'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T10:11:00Z'
updated_date: '2026-10-24T13:21:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 470000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #7: data/v2.js:5368,5426-5434 writes 1.4 (radiators) / 0.6 (underfloor) / 0.8 (heat pump), also over a stored value on migrate. Entry 24 (Sergeant D): default 2.0, stored values stay. Also covers finding #28 (default still 1.5, range 0.1-5 vs thermo-nova 0.1-12, const.py:182).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Wizard and migrate keep a stored coefficient; a fresh install gets 2.0 (entry 24); range matches thermo-nova or the deviation is put to Sergeant D (webui test + settings test)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 evidence: tests/test_sat_wizard_coefficient.py 4/4 OK on fix, 4 failures on HEAD v2.js/SATtypes.h (presets 1.4/0.6/0.8, unconditional satcoefficient write, slider not from stored value, default 1.5). Other SAT tests, settings whitelist, webui asset versioning OK; evaluate --quick 0 failed; build.bat SUCCESS 15:19. Bench (OTA, settings 178 keys unchanged): stored SATcoefficient 0.60 before and after the upgrade (0.60 itself was written by the old wizard's underfloor preset). Range 0.1-5 vs thermo-nova 0.1-12 left as is (audit #28, not asked yet). Not verified: the wizard run in a browser (Playwright MCP not connected).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Heating-curve coefficient default is 2.0 (Sergeant D entry 24, thermo-nova const.py:182); a stored value is kept. The SAT setup wizard no longer writes per-system slopes (1.4 radiators / 0.6 underfloor / 0.8 heat pump): Automatic tuning leaves satcoefficient alone, Manual writes the slope the user set and its slider starts at the stored value.
Evidence: source test 4/4 on fix, 4 failures on HEAD; build SUCCESS; evaluator 0 failed; bench stored 0.60 kept across the upgrade.
Not verified: wizard in a browser. Out of scope: coefficient range 0.1-5 vs thermo-nova 0.1-12 (audit #28).
<!-- SECTION:FINAL_SUMMARY:END -->
