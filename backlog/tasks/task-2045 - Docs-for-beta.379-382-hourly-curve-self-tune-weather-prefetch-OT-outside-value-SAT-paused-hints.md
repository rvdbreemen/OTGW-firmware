---
id: TASK-2045
title: >-
  Docs for beta.379-382: hourly curve self-tune, weather prefetch, OT= outside
  value, SAT paused hints
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T06:24:00Z'
updated_date: '2026-11-19T06:30:00Z'
labels:
  - docs
milestone: 2.0.0
dependencies: []
ordinal: 1050000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Manuals, README, MQTT.md and the 2.0.0 draft notes still describe the day-based self-tune (TASK-1889, beta.379), the MsgID 27 source without the PIC proxy answer (TASK-2040, beta.381), no weather prefetch (TASK-2039, beta.380) and no hint wording (TASK-2036, beta.382).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README and the 2.0.0 draft notes describe the hourly self-tune (12 counted hours, step 0.05) with SATcurveTune.h cited
- [x] #2 EN and NL SAT chapters name the PIC proxy answer with OT= as a MsgID 27 source, the 10 min weather prefetch and the SAT paused hint texts, matching SATcontrol.ino, SATweather.ino and i18n
- [x] #3 MQTT.md sat/outside_temp_missing names the proxy answer
- [x] #4 MANUAL.md rebuilt, test_manual_generated passes
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: README.md:98/:102 and draft :115/:173/:203 now say hourly, 12 counted hours, step 0.05; checked against SATcurveTune.h:17-28 (SAT_CT_WINDOW_DAYS 12, MIN_CLEAN_HOURS 1, HOURS_PER_DAY 1, STEP 0.05f). :203 keeps the 4-day simulation as history with a note. AC2: ch05:235/:239 and h05:263/:267 name the PIC proxy Read-Ack with OT= (OTGW-Core.ino, commit 3ff946d46: rsptype OTGW_ANSWER_THERMOSTAT, READ_ACK, not answer-override), the 10 min prefetch (SATweather.ino:618, SATcontrol.ino:2850-2858 SAT_OUTSIDE_PREFETCH_MS 600000) and the hint texts verbatim from v2.js:566-567 and i18n/nl.json:445-446. AC3: MQTT.md sat/outside_temp_missing names the proxy answer. AC4: build_manual.py wrote MANUAL.md (5813 lines), test_manual_generated OK, test_manual_sat_commands OK; no em dash in the diff. Not verified: nothing run on hardware (docs only).
<!-- SECTION:FINAL_SUMMARY:END -->
