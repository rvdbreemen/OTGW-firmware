---
id: TASK-2928
title: >-
  Web UI: the Settings SAT category expands SAT as Standalone Adaptive
  Thermostat, the SAT page and manual say Smart Autotune Thermostat
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T19:56:00Z'
updated_date: '2026-11-19T20:07:00Z'
labels:
  - webui
  - i18n
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1063000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the static i18n audit of 2026-11-19 (origin/dev 75aedd1d3). The Settings rail SAT category reads title 'SAT — Smart Thermostat' and description 'Standalone Adaptive Thermostat.' (v2.js:1023-1024 settings.cat.sat.*, v2.js:4150 category table), while the SAT page header says 'SAT · Smart Autotune Thermostat' (v2.html:332, 340; v2.js:8378) and the manuals say 'Smart Autotune Thermostat' 31 times (docs/MANUAL.md, docs/manuals/en). The packs copy the inconsistency (nl/de keep 'Standalone Adaptive Thermostat.'; de keeps the English title). One visible string pair per language; no behaviour change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Settings SAT category title and description use the same expansion as the SAT page (Smart Autotune Thermostat) in v2.js and in nl/de/el.json
- [ ] #2 i18n pack tests and i18n-no-hardcoded green; a grep shows no 'Standalone Adaptive' left in data/ and i18n/
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 (ba). Fix: settings.cat.sat.desc becomes 'Smart Autotune Thermostat.' in I18N_EN (v2.js:1023), the SET_CATS fallback (v2.js:4150) and nl/de/el.json; it is a proper name, so every language shows it as is, like the untranslated SAT page header (v2.html:340). The titles stay ('SAT — Smart Thermostat' and its translations): a shortened form, not a different expansion. Firmware grep (read-only): no 'Standalone Adaptive' in src/**/*.h|ino|cpp incl. settingsHelp.h and SATtypes.h; the firmware comments all say 'Smart Autotune Thermostat' (OTGW-firmware.h:452/834, SATcontrol.ino:4, SATpid.ino:4, SATtypes.h:8, SATble.ino:6, SATweather.ino:6). New tests/test_sat_name_consistent.py: red on origin d6544e15e (2 of 3 fail: settings category, wrong expansion left), green on the fix (3/3). Also green: test_sat_not_experimental, test_sat_handback_wording, test_update_check_settings, test_build_staging (18), evaluate --quick 0 FAIL.
<!-- SECTION:NOTES:END -->
