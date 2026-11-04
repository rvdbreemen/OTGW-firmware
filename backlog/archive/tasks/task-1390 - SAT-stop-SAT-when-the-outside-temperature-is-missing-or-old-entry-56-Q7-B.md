---
id: TASK-1390
title: 'SAT: stop SAT when the outside temperature is missing or old (entry 56, Q7 B)'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T13:17:00Z'
updated_date: '2026-10-31T05:47:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.5
references:
  - TASK-1412
ordinal: 529000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-25 (Q7: B, entry 56): when the outside temperature is missing or old, SAT stops at once. Firmware today: satGetOutsideTemp() (SATcontrol.ino:1495-1515) falls back external (10 min stale, :142, :1503) -> weather (bValid, never reset to false in src/OTGW-firmware, so a failed fetch keeps an old value forever; poll 900 s, SATtypes.h:501) -> OT MsgID 27 (0.0 when the boiler has no sensor), so with no source SAT silently heats on a curve for 0 C. Needs: a 'valid and fresh' outside temperature, a stop (hand-back like entry 47) when none, and resume when one returns. The age threshold and the hand-back form are asked as a follow-up (Q7b).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With no fresh outside temperature from any source SAT hands back once and stays silent; resumes when a fresh value returns (host test red on HEAD)
- [x] #2 Weather value expires after the agreed age (Q7b)
- [x] #3 Status shown (REST/MQTT/UI), docs updated
- [x] #4 Weather value older than 1 hour is not used (maintainer 2026-10-25); poll interval stays <= 3600 s
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 Q7b answered (entry 57): old = no fresh value from any source for 1 hour; on expiry the entry-47 hand-back once, resume when a fresh value arrives.

2026-10-25 maintainer decision (Robert): weather data must be fetched at least once per hour; for ageing, weather data may not be older than 1 hour. Consistent with entry 57. Current code: the poll interval setter clamps 900-3600 s (settingStuff.ino:1053), so 'at least once per hour' already holds for the schedule. Edge case to handle in the implementation: with the interval at 3600 s, one slow or failed fetch makes the value older than 1 h, so SAT would hand back until the next successful fetch (the TASK-1319 retry backs off 60 s doubling up to the interval, SATweather.ino:447-449). Side note: REST descriptor says min 300 s (restAPI.ino:3927) while the setter's minimum is 900 s (settingStuff.ino:1053).

2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

Field evidence 2026-10-29 (gruijter, alpha.527, TASK-1474): after a Task-watchdog reboot at 12:56:52 SAT ran with outside=0.0 in every control tick 12:57:24-13:01:24 (curve 33.2 instead of 28.0, CS 33.3-34.1) until the first weather fetch at 13:01:53. No MsgID 27 on that bus; satGetOutsideTemp falls back to the bus value 0.0 while weather is invalid (SATcontrol.ino ~1730-1733) and weatherLoop waits WEATHER_POLL_MIN_SEC = 300 s on purpose (SATweather.ino:28, ~418-420).

2026-10-29: implemented together with TASK-1482 (same validity rule); plan in TASK-1482, waits for Sergeant D's OK on the open points.

2026-10-30 close-out: implemented with TASK-1482, merged on origin/dev as f07db1ec5 (alpha.540). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Evidence collected this session on c00c5f025: python tests/test_sat_restart_quiet.py Ran 17, OK; red on f07db1ec5^ (failures=13, errors=1). AC#1: test_weather_older_than_one_hour_hands_back_once_and_resumes red on the old code (CS=34.0 keeps going at t=3601 s) and green (one hand-back CS=0/CH=1/MM=T at t=3601 s, then resume); also boot without an outside temperature stays quiet. AC#2: agreed age 1 hour (entry 57), SAT_OUTSIDE_VALID_MS = 3600000; external value valid for the whole hour (test red/green). AC#4: weather older than 1 h not used (same test); the poll interval setter clamps 900..3600 s (settingStuff.ino:1058). MsgID 27 counts only with a live boiler Read-Ack (tests red/green). AC#3 left open: the status is in REST (outside_temp_missing field), MQTT (sat/outside_temp_missing, retained) and the v2 UI hint (satOutsideMissing), and MQTT.md plus EN/NL manuals are updated, but the evidence is source-level only (test_status_published) and no webui test or screenshot shows the hint rendered: to do after the batch flash on the bench. This task still declares a dependency on TASK-1412.5 (To Do); not changed here.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

2026-10-31 bench (session otgw-firmware-ea), OTGW32 .61 on 2.0.0-alpha.565+a958d39: with weather disabled, no external outside value and SAT enabled, all three status surfaces show the state: REST /api/v2/sat outside_temp_missing true; MQTT retained OTGW/value/otgw-1020BA21B4F8/sat/outside_temp_missing true; v2 SAT page (real page, headless Edge over CDP on the live device) shows the hint 'No outside temperature for 1 hour: boiler handed back' visible (screenshot scratchpad shot/1390-outside-missing.png, script shot/live-hint.mjs). Docs were already updated (MQTT.md, EN/NL manuals). Observation, not changed: disabling the weather source drops its value at once (the last fetch was 21 min old), so in that path the hint's 'for 1 hour' wording is not literally true; the entry 56/57 rule applies the 1 h age to a source that is still configured. Bench restored (weather on, simulation off).

Screenshot archived as docs/evidence/task-1390-outside-missing-hint-alpha565.png.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
With no outside temperature younger than 1 hour from any source, SAT hands back once (CS=0, CH=1, MM=T) and stays silent, resuming when a fresh value returns (Sergeant D entries 56 B, 57 A). Status on REST (outside_temp_missing), retained MQTT sat/outside_temp_missing and a v2 SAT-page hint; docs updated. Evidence: host tests per the earlier notes; bench alpha.565 on OTGW32 shows the state on all three surfaces, including the rendered hint. Observation: switching weather off drops the value immediately, so the hint wording '1 hour' is not literal in that path.
<!-- SECTION:FINAL_SUMMARY:END -->
