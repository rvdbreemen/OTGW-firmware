---
id: TASK-2031
title: >-
  docs: manual text for the BLE no-PSRAM refusal (ADR-257) and the no-network
  low-heap backstop restart (ADR-258)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T15:22:00Z'
updated_date: '2026-11-19T11:00:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1042000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator request 2026-11-18. Manual text prepared before the firmware lands, pushed only after each fix is on origin/dev: (a) ba's lane ff73985 (BLE cannot be switched on without PSRAM: disabled switch, REST 409, stored on cleared at boot, ADR-257 supersedes ADR-222); (b) d3's backstop restart after about 16 min with no network and almost no heap, plus telnet key H (ADR-258).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ch05/h05 5.16 and the settings rows describe the no-PSRAM refusal as the code does (names checked against ff73985)
- [ ] #2 7.12a and the telnet key list describe the backstop restart and the H key as d3's code does
- [ ] #3 Each part pushed only after its firmware commit is on origin/dev
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
(a) prepared from ff73985: bleActive() = bBleEnable && psramFound() (SATble.ino); setter bleEnableAllowed() covers boot parse, REST and MQTT sat/ble_enable (MQTTstuff.ino:745 -> SATbleenable); REST 409 body 'BLE needs PSRAM: not available on this board' (restAPI.ino); stored true refused -> settingsDirty, saved off (settingStuff.ino); UI: switch disabled, opacity 0.55, title = values.ble_needs_psram, BLE card hint (v2.js). Version 'beta.375' in the text comes from ff73985's version.h: re-check at landing (a rebase can renumber).

2026-11-18 TASK-2025 part split off for the push after 375 (coordinator; ba's review): b0b13c7d4 (ch05/h05 :482/:484/:510) plus the symptom sentence of 7.12a in ch07:372 / h07:525 ('accepting the stability warning' limited to firmware before 2.0.0-beta.375, a stored on is cleared at boot), taken verbatim from 39ae52275, and docs/MANUAL.md rebuilt (TASK-2030 gate). Grep over docs/manuals, docs/api and README for riskack/consent/stability warning: no other old-consent text. The TASK-2026 backstop paragraph (39ae52275 rest, 787150c9c, 61e2e7f43) keeps waiting for the firmware.

2026-11-19 coordinator (Robert DM 1572927618628583465, A): stable notes draft gets a WiFi Changes bullet (ADR-258 beta.383, ADR-261 beta.385, log line beta.388; checked against networkStuff.ino:649, :821-828, :912-919) and the Known issue line verbatim: option C proven on the host only.
<!-- SECTION:NOTES:END -->
