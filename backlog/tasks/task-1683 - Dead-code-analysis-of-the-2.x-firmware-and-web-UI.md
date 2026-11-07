---
id: TASK-1683
title: Dead code analysis of the 2.x firmware and web UI
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T09:22:00Z'
updated_date: '2026-11-07T09:29:00Z'
labels:
  - analysis
  - cleanup
dependencies: []
priority: medium
ordinal: 804000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07: run a dead code analysis. Read-only analysis (no build during the beta.118 freeze): unreferenced functions, #if 0 blocks, unused globals/settings, cross-checked against the symbols of the latest archived esp32-combo .elf, plus unused functions in v2.js. Output: a ranked list with evidence; removals only after Robert's go.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every candidate has evidence: reference count from source and presence/absence in the latest .elf symbol table
- [x] #2 False-positive classes (callbacks, table-registered handlers, macros, inlined) are checked per candidate and named
- [x] #3 Report with candidates ranked by confidence, given to Robert; no code removed without his go
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Method: scanner over our own sources (src/OTGW-firmware, src/libraries/Platform, src/libraries/OTGWSerial; vendored espMqttClient/OpenTherm/SimpleTelnet only searched for references): 1644 function definitions, uses counted with comments and string literals stripped, definitions and prototypes not counted. Cross-check: demangled symbols of the beta.119+35f2cfd esp32-combo .elf (= HEAD 35f2cfd, 34003 symbols). Absence from the elf only supports a zero-use finding (single-caller functions are inlined and gc-sectioned too, so 'used but not in elf' is no signal). Transitive pass: functions whose every use lies inside a dead body.

Result firmware, 25 zero-use functions, all absent from the elf: FSexplorer formatLittleFS, freeSpace; MQTTstuff.h pgm_strncmp_PP, pgm_read_char; MQTTstuff sendMQTT, publishMQTTInt (2 overloads); OTGW-Core.h isDHWActive; OTGW-Core overrideKindLabel; OTGW-firmware.h canReplayOTGWSimulation, isWiFiConnected, hardwareHasPIC, comboClassicPinsActive; OTGW-firmware.ino blinkLEDms; SATble bleMatchesConfiguredMAC (static); SATcycles satCycleCountClass; helperStuff replaceAll; jsonStuff sendStartJsonObj, sendEndJsonObj, sendStartJsonMap, sendEndJsonMap, sendJsonOTmonMapEntryDallasTemp (2 overloads); Platform platformWiFiClientSetSync, platformTaskExit. Transitive: formatBytes, restSendP, restFlushContent (only called from the dead jsonStuff/FSexplorer code). False positive excluded: OTGWUpgrade::~OTGWUpgrade (implicit destructor call).

False-positive checks: no ## token pasting builds these names (only safeTimers.h timer macros); tests/: sendMQTT appears only as a string in test_evaluate.py and as the evaluate.py ADR-102 pattern gate; platformTaskExit only in a test that asserts it is NOT called; bleMatchesConfiguredMAC has a lifted copy in tests/test_ble_parsers.cpp (the test re-implements it, so it does not exercise the firmware function). No #if 0 blocks; ESP8266 appears only in comments of OTGW-ModUpdateServer.h and two others. v2.js: 359 functions, 0 unreferenced; detector proven by a planted probe (2 fake functions found).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC3 done. 25 zero-use firmware functions (+3 transitive) found, each with 0 source uses and absent from the beta.119 elf; false-positive classes checked (destructor, token pasting, tests, strings); v2.js clean (359 functions, detector proven with a planted probe); no #if 0, ESP8266 only in comments. Report given to Robert; he chose removal of the firmware functions while keeping the 2 Platform shims, tracked as a follow-up task after the beta.118 freeze. No code removed in this task.
<!-- SECTION:FINAL_SUMMARY:END -->
