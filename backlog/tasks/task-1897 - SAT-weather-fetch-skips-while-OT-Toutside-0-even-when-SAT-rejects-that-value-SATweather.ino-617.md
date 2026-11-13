---
id: TASK-1897
title: >-
  SAT weather fetch skips while OT Toutside != 0 even when SAT rejects that
  value (SATweather.ino:617)
status: To Do
assignee: []
created_date: '2026-11-13T10:07:00Z'
updated_date: '2026-11-13T10:53:00Z'
labels:
  - bug
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 966000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1896 (reporter indigo_light, https://discord.com/channels/812969634638725140/815561033036333076/1570739066360561805: weather compensation gets no data after entering coordinates). weatherLoop() skips every fetch on 'if (OTcurrentSystemState.Toutside != 0.0f) return;' (SATweather.ino:617, added by TASK-511, f12a753eb). A gateway-made MsgID 27 value (here a DS18B20 on the PIC, boiler without MsgID 27) fills OTcurrentSystemState.Toutside, but satResolveOutsideTemp() (SATcontrol.ino:2546-2569) rejects it because only a live boiler Read-Ack stamps state.otBus.iToutsideBoilerMs (OTGW-Core.ino:6098-6102). Result: no weather fetch, no log line, fetch_errors 0, and SAT has no valid outside source (outside_temp_missing). Fix: gate :617 on the same freshness as SAT, i.e. skip only while state.otBus.iToutsideBoilerMs is non-zero and younger than SAT_OUTSIDE_VALID_MS. No Sergeant D decision needed: weather is already an entry-57 source; only the skip condition was wrong. The question whether a gateway-measured value itself counts stays in TASK-1896.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real weatherLoop gate: red on the old code (fetch skipped with a gateway-made Toutside and no fresh boiler stamp), green on the fix (fetch requested); still skipped with a fresh boiler MsgID 27
- [ ] #2 Field confirmation by the reporter after the fix build
- [ ] #3 Bench: with Toutside non-zero but no MsgID 27 stamp younger than 1 h (sensor gone, or before the first stamp), /api/v2/sat/weather valid=true after the first fetch and the SAT outside source is the weather (osrc=W)
<!-- AC:END -->
