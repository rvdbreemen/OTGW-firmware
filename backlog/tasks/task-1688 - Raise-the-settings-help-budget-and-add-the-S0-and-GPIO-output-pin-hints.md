---
id: TASK-1688
title: Raise the settings help budget and add the S0 and GPIO output pin hints
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T11:05:00Z'
updated_date: '2026-11-08T09:03:00Z'
labels:
  - webui
  - docs
dependencies: []
priority: low
ordinal: 808000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: raise the settings-help budget. MAX_BYTES 18 KB in tests/webui/settings-help.test.mjs dates from when settings-help.json lived on LittleFS; since TASK-1294 (ADR-194) it is PROGMEM in the app slot (2,293,760 B, beta.118 uses 2,123,984 B) and is streamed from flash without a RAM copy (webSendPStream). Raise to 24 KB, fix the stale LittleFS comment in settingsHelp.h, and add the GPIO-not-hole hint to s0counterpin and gpiooutputspin (TASK-1662 follow-up).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 settings-help test limit 24 KB with the reason in the test comment, test passes
- [x] #2 s0counterpin and gpiooutputspin help name the GPIO-not-hole rule
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done 2026-11-08: tests/webui/settings-help.test.mjs MAX_BYTES 24 KB with the reason (PROGMEM in the app slot since TASK-1294/ADR-194, streamed from flash, no RAM copy); new test 'the sensor, S0 and output pin help say GPIO number, not the hole' (all three keys: GPIO number, not the hole, a D-hole to GPIO example). settingsHelp.h header comment no longer describes a LittleFS file. Help now 166 keys, 17128 B. Old header (HEAD copy via OTGW_SETTINGS_HELP): 28 pass 1 fail; new: 29 pass 0 fail. evaluate --quick 0 failed. Help text deliberately names no output default: TASK-1698 (per-board defaults) is in progress elsewhere.

Rebase onto TASK-1698 (0bdd8269c, per-board S0/output defaults): the 1688 pin help had replaced 1698's default text. Merged: output and S0 help say GPIO number, not the hole, and name the Classic defaults (output D7 = GPIO 11 on the S3 Mini / D8 = 37 on the Pro; S0 D6 = GPIO 13 / 39); the sensor example moved to D8 = GPIO 10 so it does not collide with the defaults. settings-help test 29/29, 17.3 KB of 24 KB.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: settings-help test limit 24 KB with the PROGMEM/ADR-194 reason, 29/29. AC2: s0counterpin, gpiooutputspin (and gpiosensorspin) say GPIO number, not the hole, with the TASK-1698 Classic defaults kept (merge fix 060c8383b). Pushed f6b71e918 + 060c8383b (beta.140-141); bench .61 /settings-help.json serves them; #alpha-testing 1568913428758798336 (zip beta.141+963b68d covers 137-141).
<!-- SECTION:FINAL_SUMMARY:END -->
