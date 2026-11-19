---
id: TASK-1977
title: >-
  Upstream PR to the OpenTherm library (Phunkafizer fork): isReady() wrap after
  2^31 us without a valid answer, setDelay() wrap-safety, TX_DATA timeout
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T15:19:00Z'
updated_date: '2026-11-15T15:35:00Z'
labels:
  - upstream
dependencies: []
priority: medium
ordinal: 1010000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert DM 1571540335740325931 (answer A to 1571539423378538590): one upstream PR to https://github.com/Phunkafizer/opentherm_library with the three fixes found in TASK-1966. Library style, minimal. Repro in the PR text (repo has no tests); host evidence locally. English, neutral, no project-internal names. Our OTDMasterLine workaround stays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fork + branch on rvdbreemen, fix on current upstream master, compiled
- [x] #2 Host repro on the real OpenTherm.cpp with fake micros(): old stalls after 2^31 us without a valid answer, fix keeps isReady(); setDelay across a wrap
- [x] #3 PR opened, link sent to the coordinator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 session 95: fork rvdbreemen/opentherm_library already existed (same fork network, parent ihormelnyk); branch fix-timer-wrap on Phunkafizer master 720cee9, commit 51aa68e (src/OpenTherm.cpp +27 -2): process() pins an elapsed delayTimestamp to micros(); setDelay() signed compare; end() resets txStatus/txIndex after stopping the tx timer; process() ends a TX_DATA frame whose timer has not fired for 10 ms (TIMEOUT). Upstream line numbers: isReady :149-154, setDelay :426-430, DATA_READY deadline :509, WAIT_RESPONSE timeout :526. Host repro (scratchpad ot-host/repro.cpp, real OpenTherm.cpp, MSVC 32-bit unsigned long, stub Arduino.h + gptimer, SOC_GPTIMER_SUPPORTED 1): master 720cee9 FAIL 4/4 (case 1 first refusal at 2147.8 s, case 2 delay dropped, case 3 TX_DATA after end()+begin(), case 4 TX_DATA after a timer stall); branch PASS 4/4. Compiled on the host only, not in an ESP32 toolchain (one-build rule; our tree keeps the pinned library and the OTDMasterLine workaround). PR: https://github.com/Phunkafizer/opentherm_library/pull/1 (open, base master, head 51aa68e).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: branch fix-timer-wrap on rvdbreemen/opentherm_library from Phunkafizer master 720cee9, commit 51aa68e; host-compiled (MSVC, unchanged OpenTherm.cpp), no ESP32 target build (stated in the PR). AC2: host repro red on 720cee9 (4/4 fail, first refusal at 2147.8 s = 2^31 us + loop step) and green on 51aa68e (4/4 pass). AC3: https://github.com/Phunkafizer/opentherm_library/pull/1 open, link sent to the coordinator. Merge is up to the upstream maintainer; our workaround stays.
<!-- SECTION:FINAL_SUMMARY:END -->
