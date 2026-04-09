---
id: TASK-111
title: 'SAT Audit C3: NTP and time management'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:52'
updated_date: '2026-04-09 05:24'
labels:
  - audit
  - sat
  - phase-3
milestone: m-0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verify NTP time management in SAT C++ firmware: timezone handling, time-dependent functions (schedule-based presets) and behavior during NTP outage.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 NTP integration verified
- [x] #2 Timezone handling correct
- [x] #3 Time-dependent SAT features work without NTP (graceful degradation)
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for deviations
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited NTP and time management for SAT.

NTP implementation:
- Both platforms use configTime(0, 0, ntpHost) + SNTP for time sync. This is identical on ESP8266 and ESP32 — no platform branching needed.
- Timezone handling uses AceTime library (ZonedDateTime, timezoneManager) consistently on both platforms. No platform-specific timezone code exists.
- isNTPtimeSet() is available and used by the firmware; SAT does NOT call isNTPtimeSet() — see below.

SAT time dependency:
- SAT has NO schedule-based setback, no night mode, no time-of-day presets. All preset changes are event-driven (MQTT command, window detection, Home Assistant automation). SAT does not use hour(), localtime(), or any NTP time internally.
- Energy accumulation (fEnergyTotal) uses millis() delta, not wall-clock time — no NTP dependency.
- Cycle timing uses millis() throughout — no NTP dependency.
- Weather poll uses DECLARE_TIMER_SEC() / DUE() — also millis()-based.

Graceful degradation without NTP:
- Because SAT has no time-based features, NTP outage has zero functional impact on SAT operation.

Issues found:
- TASK-201: startNTP() has an ESP8266-only hostname workaround without a platform guard (cosmetic/maintainability issue, no runtime impact).
- No functional NTP gaps for SAT specifically.
<!-- SECTION:FINAL_SUMMARY:END -->
