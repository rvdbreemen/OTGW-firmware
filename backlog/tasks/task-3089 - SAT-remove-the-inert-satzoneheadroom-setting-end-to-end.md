---
id: TASK-3089
title: 'SAT: remove the inert satzoneheadroom setting end to end'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T19:19:00Z'
updated_date: '2026-11-26T09:31:00Z'
labels:
  - sat
  - settings
  - webui
milestone: 2.0.0
dependencies: []
ordinal: 1159000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert decided (interactive, via the coordinator, 2026-11-25): satzoneheadroom is removed. With at most 4 zones (satzonecount 1 to 4) the 75th percentile is the highest zone output, so min(highest, P75 + headroom) never binds for any headroom >= 0 (SATcontrol.ino satAggregateZoneSetpoint; derived in firmware-sat-spec.md). The setting is inert and its help text said so (TASK-3018). Remove it from the settings struct, settings.ini read/write, REST GET and POST whitelist, web UI field, help text, i18n packs nl/de/el, manuals, REST/spec docs and tests. The aggregation keeps thermo-nova's fixed 5.0 as a constant (area.py:267, :298-299), so SAT behaviour does not change (agrees with Sergeant D entry 116: port area.py as it is). An old settings.ini that still has SATzoneheadroom must load without error or migration noise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No reference to satzoneheadroom / SATzoneheadroom / fZoneAggregationHeadroom is left in src/, data/, i18n/, scripts/json-golden, docs manuals and tests, except the removal test and changelog
- [x] #2 Old settings.ini with SATzoneheadroom loads silently (no unknown-key error, no dirty flag, next save drops the key); REST POST of the name answers 400 Unknown setting name
- [x] #3 SAT behaviour unchanged: satAggregateZoneSetpoint host test green with the constant, 1 to 4 zones
- [x] #4 Red/green: removal test fails on origin/dev, passes on the fix; mutant (re-adding the key) fails a test
- [x] #5 evaluate --quick no new FAIL; webui suite one file at a time; relevant python tests green; otgw-dev diff review
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision: Robert (interactive via the coordinator, 2026-11-25). otgw-architect 2026-11-25: no ADR needed (no ADR names the setting; ADR-051 pattern untouched; precedent OTDsetbacktimeout TASK-1950); spec lines rewritten, CHANGELOG line under Unreleased/Removed added.

Evidence: tests/test_sat_zone_headroom_removed.py 8 pass on the fix; with OTGW_SRC_REV=origin/dev 15+ subtests fail (key still in 6 source files, i18n nl/de/el, golden, REST whitelist, helper signature). Mutants fail: key back in knownSettings[] (2 fail), SATzoneheadroom branch back in updateSetting (2 fail). Old-settings.ini compat is a SOURCE check (updateSetting has no branch, no unknown-key error, no-op return, boot parse clears dirty, next save drops the key); updateSetting needs the whole firmware so it is not run as a parser; to be confirmed on the bench by loading a settings.ini that still has SATzoneheadroom. test_sat_zone_aggregation.py (16 pass) runs the real helper with the constant 5.0, 1 to 4 zones; test_sat_published_curve, test_settings_whitelist, rest_bounds, rest_range_matches_clamp, float_roundtrip, fs_image_settings_defaults, progmem_assets_deflated, sat_comfort_adjust_removed, flash_migrate_settings_compare green. evaluate --quick 88 pass/0 fail; webui suite 108 files fail 0. Left on purpose: docs/design/boiler-dashboard-concepts.html (design mock-up) and the historical CHANGELOG lines. Not built (otgw-dev builds).

R1 final base 890b08680: evaluate --quick 100 % (89 pass, 0 fail); full run_tests.py 377/380, remaining 2 = README governance (fixed, 17/17) and a host-compile timeout under load (test_sat_loop_integration 21/21 alone).
<!-- SECTION:NOTES:END -->
