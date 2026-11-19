---
id: TASK-2039
title: >-
  SAT: weather is not prefetched before MsgID 27 expires, so all outside sources
  go stale at once and SAT hands back mid-burn (marceld beta.361)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T01:07:00Z'
updated_date: '2026-11-19T01:24:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1045000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field reproduction: marceld91d, beta.361 (Remeha Calenta + Toon), capture 20261117-081252-upload.zip (#alpha-testing 1572501476121641060), 2026-11-17 10:11:35 local: 10.9 s after ignition the telnet log shows 'SAT: no outside temperature younger than 1 h, boiler handed back (CS=0, CH=1, MM=T)', 0.4 s later 'SAT: outside temperature valid again, SAT resumes' (source O->W), then SAT resumed in CONTINUOUS (mode=1, CS 26.0, MM=100) with the flame burning until the flame-out fail-safe moved it to PWM at +48 s. A mid-burn CS=0 on a real boiler caused by the outside-age check crossing 1 h exactly when the outside source switched. Found by otgw-bench-tester's second read for TASK-1942 (notes: 09 scratchpad t1942/second-read-1942.md). Possible second case: gruijter on beta.375 saw 'No outside temperature for 1 hour: boiler handed back' right after the update (#alpha-testing 1572711887722778637), not yet confirmed as the same cause. Code: SATcontrol.ino:4476-4492 (outside-temperature gate, satOutsideTempFresh, bOutsideTempMissing), entries 56/57 (hand-back without an outside temperature). thermo-nova has no hand-back: it skips the control pass while the outside temperature is None (climate.py:544-545). Whether a resume with the flame burning is a heating start is asked separately (Q-1942a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The hand-back still fires when no outside source at all is younger than 1 h (entries 56/57 unchanged); host test
- [ ] #2 Bench or replay run shows no CS=0 hand-back at an outside source switch
- [x] #3 A host test replays marceld's sequence (MsgID 27 fresh, weather not fetched because of it, MsgID 27 stamp reaching 1 h during a burn) on the real code: red on the old code (hand-back + resume), green on the fix (weather prefetched before the MsgID 27 stamp reaches 1 h, no hand-back)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 cause corrected by otgw-dev (coordinator's decision, title changed): satResolveOutsideTemp() (SATcontrol.ino:2850-2875) already hands back only when NO source is fresh (entry 57); the gap is that weatherLoop() skips the weather fetch while MsgID 27 is fresh (SATweather.ino:616, TASK-1897), so the weather value goes stale too and both expire together; the fetch only runs after the hand-back. Fix direction: prefetch the weather before the MsgID 27 stamp reaches 1 h. AC1 replaced accordingly (the old AC1 described behaviour the code already has). Owner: otgw-dev.

2026-11-19 otgw-dev: cause confirmed against marceld's capture (20261117-081252-upload, hours 08-10, telnet + otframes): weather_valid false for the whole hour before; hand-back 10:11:45.96 'SAT: no outside temperature younger than 1 h'; the FIRST weather fetch of the hour at 10:11:46.10 (valid 10:11:46.40), 'outside temperature valid again' 10:11:46.41, osrc O -> W. weatherLoop() skipped every fetch while MsgID 27 was fresh (SATweather.ino:616, TASK-1897), so the fallback was never warm. Side finding: marceld's outside value comes from HA as OT= (set/otgw-D/outside), the PIC answers the thermostat with A frames (230 MsgID 27 frames, all T/A, no B/R), and by design (TASK-1896) an A frame never stamps MsgID 27; the one stamp ~09:11:45 is from before the capture start (boot ~09:05, uptime 450 s at 09:12:55), source not determined. Whether an HA OT= override should count as a fresh source is a separate design question (not in this task). Fix (coordinator: option 1, prefetch): satOtOutsideFreshAhead() (SATcontrol.ino) is true while the MsgID 27 stamp stays fresh for at least SAT_OUTSIDE_PREFETCH_MS 600 s more; weatherLoop() skips only then, so in the last 10 min the normal fetch path runs (first-fetch path when weather was never valid, or the overdue 900 s poll timer). The resolver (entry 57) is unchanged. API-call cost vs TASK-1897 (Robert's B on its AC3): zero while the boiler keeps refreshing MsgID 27 (the stamp never reaches 50 min); when MsgID 27 stops refreshing, weather polling starts 10 min earlier than it did after the expiry anyway, so at most one extra fetch per expiry. Evidence: tests/test_weather_worker_handoff.py C7 (near expiry + weather never valid -> fetch; > 10 min left -> still skipped) FAILS on origin SATweather.ino, passes on the fix; tests/test_sat_outside_order.py OutsidePrefetch (real helper: ahead at 49:59, not at 50:00/59:59; AC1: every source > 1 h -> none, so the hand-back still fires) fails on origin SATcontrol.ino (helper missing), passes on the fix; related files 64 passed; evaluate --quick 89/0. Not built, not bench-tested.

2026-11-19 side finding queued as Q-2039a (92 -> Sergeant D): should an OT= outside value (HA via set/<node>/outside, MQTTstuff.ino:421/512) count as a fresh SAT source? On marceld the PIC answered MsgID 27 itself (all 230 frames T/A, no B/R; gateway.asm 6.6:3050-3067 UserOutsideTemp), and an A frame never stamps (OTGW-Core.ino:5241-5247, TASK-1896), so SAT ran on the weather. Options A (count as fresh O for 1 h after each OT=), B (unchanged), C (count as E, external, ranking first; thermo-nova uses the user's HA entity first, climate.py:317-343). Our advice C, scope 2.0.0. Not part of this fix.

2026-11-19 correction: Q-2039a is withdrawn. Entries 330 (Robert) and 331 (Sergeant D) already decided that an OT=/TS=O value on MsgID 27 counts at the MsgID-27 rank; I had not checked the ledger before writing the options. marceld's case is an implementation gap against 330: filed as TASK-2040 (2.0.0, high). Option C (rank OT= first as external) is off the table unless Robert wants it.
<!-- SECTION:NOTES:END -->
