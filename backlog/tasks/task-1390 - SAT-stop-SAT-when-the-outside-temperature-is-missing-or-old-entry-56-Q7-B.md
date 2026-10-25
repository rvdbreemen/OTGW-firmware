---
id: TASK-1390
title: 'SAT: stop SAT when the outside temperature is missing or old (entry 56, Q7 B)'
status: To Do
assignee: []
created_date: '2026-10-25T13:17:00Z'
updated_date: '2026-10-25T13:46:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 529000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 (Q7: B, entry 56): when the outside temperature is missing or old, SAT stops at once. Firmware today: satGetOutsideTemp() (SATcontrol.ino:1495-1515) falls back external (10 min stale, :142, :1503) -> weather (bValid, never reset to false in src/OTGW-firmware, so a failed fetch keeps an old value forever; poll 900 s, SATtypes.h:501) -> OT MsgID 27 (0.0 when the boiler has no sensor), so with no source SAT silently heats on a curve for 0 C. Needs: a 'valid and fresh' outside temperature, a stop (hand-back like entry 47) when none, and resume when one returns. The age threshold and the hand-back form are asked as a follow-up (Q7b).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With no fresh outside temperature from any source SAT hands back once and stays silent; resumes when a fresh value returns (host test red on HEAD)
- [ ] #2 Weather value expires after the agreed age (Q7b)
- [ ] #3 Status shown (REST/MQTT/UI), docs updated
- [ ] #4 Weather value older than 1 hour is not used (maintainer 2026-10-25); poll interval stays <= 3600 s
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 Q7b answered (entry 57): old = no fresh value from any source for 1 hour; on expiry the entry-47 hand-back once, resume when a fresh value arrives.

2026-10-25 maintainer decision (Robert): weather data must be fetched at least once per hour; for ageing, weather data may not be older than 1 hour. Consistent with entry 57. Current code: the poll interval setter clamps 900-3600 s (settingStuff.ino:1053), so 'at least once per hour' already holds for the schedule. Edge case to handle in the implementation: with the interval at 3600 s, one slow or failed fetch makes the value older than 1 h, so SAT would hand back until the next successful fetch (the TASK-1319 retry backs off 60 s doubling up to the interval, SATweather.ino:447-449). Side note: REST descriptor says min 300 s (restAPI.ino:3927) while the setter's minimum is 900 s (settingStuff.ino:1053).
<!-- SECTION:NOTES:END -->
