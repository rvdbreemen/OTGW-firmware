---
id: TASK-2928
title: >-
  Web UI: the Settings SAT category expands SAT as Standalone Adaptive
  Thermostat, the SAT page and manual say Smart Autotune Thermostat
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T19:56:00Z'
updated_date: '2026-11-19T20:36:00Z'
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
- [x] #1 Settings SAT category title and description use the same expansion as the SAT page (Smart Autotune Thermostat) in v2.js and in nl/de/el.json
- [x] #2 i18n pack tests and i18n-no-hardcoded green; a grep shows no 'Standalone Adaptive' left in data/ and i18n/
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 (ba). Fix: settings.cat.sat.desc becomes 'Smart Autotune Thermostat.' in I18N_EN (v2.js:1023), the SET_CATS fallback (v2.js:4150) and nl/de/el.json; it is a proper name, so every language shows it as is, like the untranslated SAT page header (v2.html:340). The titles stay ('SAT — Smart Thermostat' and its translations): a shortened form, not a different expansion. Firmware grep (read-only): no 'Standalone Adaptive' in src/**/*.h|ino|cpp incl. settingsHelp.h and SATtypes.h; the firmware comments all say 'Smart Autotune Thermostat' (OTGW-firmware.h:452/834, SATcontrol.ino:4, SATpid.ino:4, SATtypes.h:8, SATble.ino:6, SATweather.ino:6). New tests/test_sat_name_consistent.py: red on origin d6544e15e (2 of 3 fail: settings category, wrong expansion left), green on the fix (3/3). Also green: test_sat_not_experimental, test_sat_handback_wording, test_update_check_settings, test_build_staging (18), evaluate --quick 0 FAIL.

Pushed 2026-11-19 as beta.399 0cd239e05 (FF on d6544e15e), zip posted (Discord 1573074261139259498). Full tests/webui sweep on this v2.js: 89/89 files 0 fails. Bench 09 on .61 (0cd239e, OTA app+fs 21:28): Settings > SAT rail description 'Smart Autotune Thermostat.' in EN, NL, DE and EL, titles unchanged; SAT page header 'SAT · Smart Autotune Thermostat' in all four; 0 JS exceptions; 5-min smoke bootcount flash+2, reboot_log only the OTA line, MQTT up. Screenshots: 09 scratchpad b399/ui/sat-cat-{en,nl,de,el}.png.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The Settings SAT category description names SAT the Smart Autotune Thermostat in v2.js (I18N_EN and SET_CATS) and in nl/de/el.json, matching the SAT page header and the manual. AC1: bench 09 on .61 beta.399 (all four languages). AC2: tests/test_sat_name_consistent.py red 2/3 on origin d6544e15e, green 3/3; i18n python tests, test_build_staging and the full tests/webui sweep (89/89) green; no 'Standalone Adaptive' left in data/ or i18n/ (the test checks it). Firmware strings checked: none used the other expansion.
<!-- SECTION:FINAL_SUMMARY:END -->
