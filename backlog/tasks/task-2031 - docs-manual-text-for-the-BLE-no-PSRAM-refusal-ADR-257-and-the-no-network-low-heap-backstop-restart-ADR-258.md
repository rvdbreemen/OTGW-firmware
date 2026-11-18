---
id: TASK-2031
title: >-
  docs: manual text for the BLE no-PSRAM refusal (ADR-257) and the no-network
  low-heap backstop restart (ADR-258)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T15:22:00Z'
updated_date: '2026-11-18T15:26:00Z'
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
<!-- SECTION:NOTES:END -->
