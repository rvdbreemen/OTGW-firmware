---
id: TASK-1542
title: >-
  SAT heat pumps: allow PWM periods of 1.5, 2 and 3 hours (field test of long
  PWM vs today's 30/40 min)
status: To Do
assignee: []
created_date: '2026-11-01T09:38:00Z'
labels:
  - sat
  - heat-pump
dependencies: []
priority: medium
ordinal: 698000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-01 (entry 193): for the TASK-1527 field test no new control mechanism; only more time. Today iHpCycleSeconds allows 1800 or 2400 s (SATtypes.h:481, setter snaps in settingStuff.ino:1050, REST limits restAPI.ino:3880, getter SATcontrol.ino:451-457, entries 40/50). Add 5400, 7200 and 10800 s as options for heat pumps (and hybrids, which use the same path per entry 188). uint16_t holds 10800. Round 14 showed the real PWM mapper works at 7200 s (docs/sat/sim-hp-lowload/round14.md); 10800 s is untested. Round 17 (round17.md) predicts fewer starts but a growing comfort cost beyond ~60 min on radiators; the field test measures it. Field test plan: docs/sat/sim-hp-lowload/field-test-plan.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Setter accepts 1800, 2400, 5400, 7200, 10800 s and snaps other values to the nearest option; REST metadata and the v2 UI choice list match
- [ ] #2 Host test against the real PWM mapper code: on/off times correct for 5400/7200/10800 s across duty ranges (red before, green after)
- [ ] #3 Existing PWM and gas tests stay green
- [ ] #4 Bench run on a unit with satsimulation at 10800 s shows a full ON/OFF period with the expected times
<!-- AC:END -->
