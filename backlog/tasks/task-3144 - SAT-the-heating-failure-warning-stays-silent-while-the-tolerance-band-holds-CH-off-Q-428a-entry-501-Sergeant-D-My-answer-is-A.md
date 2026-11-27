---
id: TASK-3144
title: >-
  SAT: the heating-failure warning stays silent while the tolerance band holds
  CH off (Q-428a, entry 501, Sergeant D 'My answer is A')
status: To Do
assignee: []
created_date: '2026-11-27T21:31:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: Robert chose option A (register entry 501, 2026-11-27 04:58:51 UTC: 'A') and Sergeant D answered the same question: #dev-sat-mqtt 1575986132716494895: 'My answer is A' (as relayed by the coordinator; otgw-docs records it as the next register entry, the number is not known yet and must exist before any code). Option A: the 15 minute heating-failure window runs only while SAT has CH enabled; inside a tolerance band pause the warning is silent and the window restarts when heating is allowed again. Known risk (options post): a real failure that starts inside a band pause is flagged only 15 minutes after CH is enabled again. Evidence of the problem: indigo_light beta.428 capture 1 (23.65 h): 4 warnings, all inside band pauses (CH 0, flame 0, duty 1.00); capture 2 (25.5 h): 2 warnings, both in real heat-ups. PLAN (nothing is implemented before the entry number exists): (1) SATtypes.h struct SatDiagIn (line ~149): add 'bool bandPause' next to chAsked. (2) SATcontrol.ino call site (dev ~8895-8910, the SatDiagIn block): set d.bandPause = _sat_tolBandOff (the same flag that already feeds satChEnableDecide on the line above and that PWM OFF phases do not set, so a PWM OFF phase does not reset the window). (3) SATcontrol.ino satDiagEventsStep (dev ~8238-8300), heating-failure part: treat bandPause like !roomValid: _sat_hfSinceMs = 0; if bHeatingFailure is set clear it with DebugTf SATEVENT heating_failure=0 reason=band_pause; return. No new event, no narration, no iHfEvents increment while bandPause holds; when the band releases the window starts at the next qualifying tick (the existing 'if (_sat_hfSinceMs == 0)' start), so the earliest flag is 15 minutes after the release. (4) Unchanged: no_ignition part (TASK-3034), entry 358 (hours with a heating-failure or no-ignition event stay out of the self-tune: with no event in a pause those hours are no longer excluded for that reason), the sat/heating_failure publish (SATcontrol.ino:5545) and the REST status field (5306), which simply read the flag. (5) Host test in tests/test_sat_diag_events.py (compiles the real satDiagEventsStep): scenario A: 20 minutes at the maximum with bandPause true and the room flat: no flag, no event count; scenario B: the same without bandPause: flag after 15 min (control); scenario C: bandPause for 10 min then released with a flat room: no flag at release+14 min, flag at release+15 min; scenario D: a flag that is set when the band starts is cleared. Red on the code before (A and C fail), green after. (6) Docs: docs/sat/firmware-sat-spec.md heating-failure paragraph and the MQTT.md note for sat/heating_failure. Order of work (coordinator 2026-11-27): after the CS-rise explanation and the telemetry R2; not before the entry number.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 tests/test_sat_diag_events.py has the scenarios A-D on the real satDiagEventsStep, red on the old code, green on the fix
- [ ] #2 While the tolerance band holds CH off no heating-failure flag, narration or event count is produced; the window restarts when heating is allowed again (earliest flag 15 min after the release)
- [ ] #3 The register entry number of Sergeant D's answer is quoted in the task notes and the docs (firmware-sat-spec.md, MQTT.md) say what silent means
<!-- AC:END -->
