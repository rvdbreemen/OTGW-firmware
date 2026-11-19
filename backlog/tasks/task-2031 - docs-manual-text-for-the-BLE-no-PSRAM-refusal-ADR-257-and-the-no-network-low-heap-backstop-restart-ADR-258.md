---
id: TASK-2031
title: >-
  docs: manual text for the BLE no-PSRAM refusal (ADR-257) and the no-network
  low-heap backstop restart (ADR-258)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-18T15:22:00Z'
updated_date: '2026-11-19T16:58:00Z'
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

2026-11-19 bundled per coordinator GO (origin a755055e5): Changes adds task 2909 (beta.390, web UI) and task 2910 (beta.389, busy-503 closes the connection under 4 KB largest block, restAPI.ino:192-194); Known issues adds task 2911 (hang after a crash boot, under investigation; normal boots 5/5 per coordinator; supervisor arms only after an answering gateway, networkStuff.ino:751/:773).

2026-11-19 (coordinator, next beta): CHANGELOG [Unreleased] filled for beta.393 (task 2918: update-check question is a header chip, not a modal; 'New version available' notice visible at <= 480 px) and beta.394 (task 2026 heap-hold hook, prerelease-only diagnostics). Stable draft Web UI bullet and manual ch03/h03 'Check GitHub for new versions' updated. Source: ba's c9b4c35b6 (lane/ba-2023, v2.js updAsk, strings 'Check for new versions?', 'Yes, check', 'No'; update-check-github.test.mjs 14/14, old v2.js 3 fail) and 5e's a02399d3a (lane-5e-heaphold, handleDebug.ino; test_heaphold_hook.py 8/8). Both not on origin at 3fb853b04; beta numbers 393/394 per coordinator, to recheck after they land. Gates: build_manual --check exit 0, test_manual_* 10 passed, no new em dashes.

2026-11-19 later: coordinator flipped the numbers: heap-hold hook (task 2026) = beta.393, update-check chip + phone notice (task 2918) = beta.394 (popover fix after 09's FAIL at 390 px). CHANGELOG, stable draft and ch03/h03 swapped; build_manual --check exit 0, test_manual_* 10 passed.

2026-11-19 final: ba's ffe5dffeb on origin bumps beta.393 (task 2918, incl. the popover-on-phone fix of an unreleased build, so no user note). Notes set to beta.393 for task 2918; the heap-hold line removed from CHANGELOG [Unreleased] until 5e lands (394 or later). build_manual --check exit 0, test_manual_* 10 passed.
<!-- SECTION:NOTES:END -->
