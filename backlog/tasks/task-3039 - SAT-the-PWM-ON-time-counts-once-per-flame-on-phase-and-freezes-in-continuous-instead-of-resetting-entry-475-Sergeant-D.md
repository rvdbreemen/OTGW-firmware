---
id: TASK-3039
title: >-
  SAT: the PWM ON time counts once per flame-on phase and freezes in continuous
  instead of resetting (entry 475, Sergeant D)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-24T18:27:00Z'
updated_date: '2026-11-25T11:34:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1147000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D, #dev-sat-mqtt 2026-11-24 (entry 475, private register): during one flame-on phase the PWM ON clock is one clock. A switch PWM to continuous freezes it (no reset); a later switch back to PWM continues from the frozen value, so with 3 or 4 flips the total PWM ON time of that flame-on phase is shortened instead of restarting each time. Scope per Robert: release 2.0.0. PENDING: Sergeant D's answer to Q-475a (A only PWM time counts, recommended / B all flame time / C other) before any code. Branch when released: sat/pwm-on-freeze off origin/dev, no merge into dev without Robert's yes. Plan: see the Implementation Plan of this task.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Sergeant D has answered Q-475a and the answer is recorded as an entry in the decisions register before any code
- [x] #2 Host test: a scripted flame-on phase with 3 PWM/continuous flips is red on origin/dev (the ON timer restarts at each re-entry) and green on the fix (the ON timer freezes and resumes)
- [x] #3 Host test: the clock resets when the flame goes out, when an ON phase ends normally, and when SAT is disabled or the heater goes off; DHW draw time still does not count (entry 95 unchanged)
- [x] #4 The start gate (entries 385/387, ADR-252), the 448/456 exit rule, the at-max rule (entry 463) and the entry 428/429 per-flame counting are unchanged (existing tests green)
- [x] #5 evaluate.py --quick shows no new FAIL; full tests/ run green; esp32-combo build OK
- [ ] #6 Bench on .63 (bench-tester): a flame-on phase with several PWM/continuous flips shows a shorter total ON time per flame than before; field proof by Sergeant D's testers
- [ ] #7 ADR (adr-kit, Proposed) records the freeze rule and cites the entry
- [x] #8 The min burn (180 s, 600 s heat pump) is read from the real flame-on of the flame across flips (entry 479); host test: a resume with little ON left ends after the frozen ON clock, not 180 s later
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
STATUS 2026-11-25: DECIDED, fully specified (entries 476, 478, 479, release 2.0.0). Code goes on branch sat/pwm-on-freeze off origin/dev, no merge into dev without Robert's yes. This text was written before the answers; the decisions are folded in below. All firmware citations are origin/dev 9f0ccba38 (src/OTGW-firmware, SATcontrol.ino = C, SATcycles.ino = Y), opened in this session. Read-only; nothing built or tested.

1. WHAT TO CHANGE (Sergeant D, entry 475): in one flame-on phase the PWM ON clock is one clock. PWM -> continuous freezes it (no reset); the next PWM entry continues from the frozen value.

2. WHERE THE ON CLOCK IS RESET TODAY
- The ON clock is _pwm_lastUpdateMs (C:1245; the one anchor of ON time and OFF wait). The ON phase ends in satPwmSchedule() when millis() - _pwm_lastUpdateMs >= onTimeMs (C:1783, :1798 "ON time expired"), after the 180 s minimum burn counted from _pwm_flameOnMs (C:1797).
- Every mode change calls satSetControlMode() (C:1975). On a change it calls satPwmReset() (C:1985), which sets _pwm_lastUpdateMs = now, _pwm_flameOnMs = 0 and clears the phase, the DHW hold and the ignition wait (C:1936-1969). It also clears _sat_startGate (C:1984) and sets _pwm_entryPending (entry 43: the first schedule step in PWM starts ON at once).
- A PWM re-entry therefore opens a fresh ON phase in satPwmBeginOn() (C:1435): _pwm_lastUpdateMs = now, _pwm_flameOnMs = now when the flame burns (C:1441, :1451). The ON time starts again from zero. This is the restart Sergeant D wants to stop.
- The same clock is also reset by: satPwmEndOn() (ON phase over, C:1494-1506), satPwmDisableOnHeaterOff() (C:2013-2021), the missing-flow path in satPwmSchedule() (C:1646), the safety-trip path (C:4397) and SAT init (C:6458).
- Precedent for freezing: hot water (entry 95/103/132): satPwmOnDhwStart() stores _pwm_dhwUsedMs = millis() - _pwm_lastUpdateMs (C:1520), satPwmOnDhwEnd() restores _pwm_lastUpdateMs = millis() - _pwm_dhwUsedMs (C:1599, :1616). The draw time does not count.

3. HOW FLIPS HAPPEN TODAY (needed to understand the change)
- ENTRY 429, exact text. Sergeant D's own words (Q-2005b, 2026-11-19 11:17 UTC): "My answer is B. But instead of flow above 3.5 change it to flow above 3." Option B as put to him (open-questions.md row Q-2005b; the Discord post itself not re-read today): "also at most one PWM entry per flame, continuous until flame-off except the 3.5 C/30 s fail-safe (entries 288/289/359)". The register's Settled line (2) reads: "The only way back to PWM during that burn is the fail-safe". So the fail-safe is an EXPLICIT exception in the option he chose, but not in his own typed words; his own words only move its threshold (3.5 -> 3.0), which presupposes it stays. Entry 417 (Q-410a A) additionally gates the fail-safe by heating allowed. Result: flips inside one flame can only be fail-safe re-entries, and entry 475's request applies to exactly those. Not a gap in the code; a point to confirm with Sergeant D in Q-475a ("do you mean the fail-safe flips of entry 429?").
- PWM -> continuous: satCycleCheckPwmExits() (Y:1481): underheat (Y:1495; on origin/dev still the 60 s rule, TASK-3008 on lane/d3-underheat changes it to entries 448/456 and does not touch the clock) and saturation (Y:1510); the at-max rule; the end-of-cycle verdict (Y:1040-1046). Each of the first two sets _cycle_burnLeftPwm = true (Y:1498, :1512).
- continuous -> PWM inside the same flame: after _cycle_burnLeftPwm the 2.0 C / 60 s rule waits for the next flame (entry 429; Y:1420-1431). The ONLY way back inside that flame is the fail-safe (flow - request >= 3.0 C for 30 s, Y:41-42, :1434-1441). A flame-out clears the flag (satCycleNoteBurnEnd, Y:193, called from Y:1121). So the 3-4 flips of a flame-on phase are fail-safe re-entries. The manual control_mode command and satPwmFirstStart() (C:4178, a heating start) can also enter PWM; a start enters when the flame is not yet burning.

4. MINIMAL CHANGE (after Q-475a = A)
- One accumulator in C: _pwm_onUsedMs ("ON time already used in this flame-on phase", 0 = none) plus a flag that it belongs to the burning flame.
- Freeze: in satSetControlMode() on PWM -> other, BEFORE satPwmReset() (C:1985): if an ON phase is running with the flame burning and not waiting for ignition, _pwm_onUsedMs = (DHW draw running ? _pwm_dhwUsedMs : millis() - _pwm_lastUpdateMs). Because the anchor is set back at each resume (below), this value is already the sum over all earlier PWM segments of the flame.
- Resume: in satPwmBeginOn() (C:1435), when the flame burns and the accumulator is set: _pwm_lastUpdateMs = millis() - _pwm_onUsedMs, set after the existing assignment at C:1440 (the same trick as C:1599). The existing check elapsed >= onTimeMs (C:1798) then ends the ON phase when the frozen time already covers the ON time asked at that tick. The ON time stays the schedule's value of each tick (our interpretation 2 in entry 475), not a stored value.
- No change to: satPwmReset() (it must NOT clear the accumulator, it runs on every mode change), the start gate (C:975-984, inside it no exit fires, so no flip, no freeze), entry 428 counting (Y:201), the hour limit, the exit rules.

5. WHEN THE ACCUMULATOR RESETS (clear it, nothing else)
- The flame goes out: next to satCycleNoteBurnEnd() (Y:1121). A DHW-only flame follows the same flame-edge hook as today; to check in the host test whether a CH flame-out during a draw reaches it.
- The ON phase ends by its own timer: in satPwmEndOn() (C:1494). The next ON phase is a new one.
- SAT disabled / heater off / restart: satPwmDisableOnHeaterOff() (C:2013) and satDisable().
- Waiting for ignition when PWM is left (no flame burned yet): nothing used, accumulator 0.
- NOT reset by a hot-water draw: the draw time never counts (entry 95), the clock stays as frozen.

6. EDGE CASES (decide with the answer)
- Frozen time >= the ON time asked at resume: the ON phase ends on the first tick after the minimum burn. Then CH off at once (OFF phase, entry 43/ADR-250 rules apply as today).
- The 180 s minimum burn (C:1797, pwm.py:201): measured from _pwm_flameOnMs, which satPwmBeginOn sets to now at the re-entry. If it stays per segment, a resume with little ON time left still burns 180 s more, which weakens the intent. DECIDED (entry 479, Q-475b A): the minimum burn (180 s; 600 s heat pump, entry 197) is counted from the real flame-on of the flame (_sat_chFlameOnMs, C:8123), across flips, not per PWM segment. A resume with little ON time left ends when the frozen ON clock ends (476/478). Implementation: the min-burn test at C:1797 must read the flame-on of the flame, not the per-segment _pwm_flameOnMs that satPwmBeginOn sets to now. This is Q-475a point 3.
- Heat pump (minOnMs 600 s, C:1796-1797): same rule.
- Start gate / 448 exit: unchanged. A flip needs the exit rule to fire, which is gated for 60 s after the first CH flame-on (ADR-252).
- Telemetry (TASK-1880, satPwmPhaseOpen/Close, C:1441, :1495): stays per PWM segment; add the used time to the open record only if cheap.
- MM=T / at-max (entry 463) and the flame-off hold: unaffected, they read bPwmFlameRequested.

7. THERMO-NOVA - VERIFIED AGAINST THE PIN 7beaaf5 on 2026-11-25 (fetched from github.com/Alexwijn/SAT into other-projects/SAT-releases-thermo-nova, HEAD 7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb, 2026-03-18 "Improve pressure sensor"). pwm.py:87-97 reset() body (_last_update = timestamp() at :91; _flame_on_at is cleared one line further, :98), :106-108 disable() calls reset(), :134 elapsed = timestamp - self._last_update, :201 flame-on hold from _flame_on_at, :205 if elapsed >= on_time_seconds. All as cited in entry 475. PWM and continuous are chosen outside this class, so thermo-nova clears the ON clock whenever PWM is left. DIFFERENCE: ours keeps it across PWM stints inside one flame, deliberate (entry 475/476 outrank the pin). Correction of my 2026-11-24 note: the HEAD 770aa45 I quoted for the other copy was the enclosing OTGW-other-projects repo, not a SAT clone; its files are content-identical to the pin (diff -rq empty).

8. SERGEANT D ENTRIES RELIED ON (decisions register, entry numbers only)
- 475 (the request; its own check says "no conflict"). 95/103/132 (DHW freeze: precedent, unchanged). 385/387 + ADR-252 (start gate, unchanged). 428 (one counted PWM ON per flame, unchanged). 429 (settled reading b: one PWM entry per flame, fail-safe the only way back; this plan relies on 429(2)). 448/456/463 (exit rules, unchanged). 43 (first PWM phase is ON).
- CHECK FOR THE SECOND READER: entry 475 calls 428's reading (b) "open"; entry 429 settled it on 2026-11-19 (Q-2005b B). No conflict with this plan (flips remain through the fail-safe), but the sentence in 475 is stale and its "Q-475a" should mention 429.

9. HOST TEST DESIGN (tests/test_sat_pwm_on_freeze.py)
- Harness: the real PWM block of SATcontrol.ino cut out and compiled as tests/test_sat_pwm_anchors.py / test_sat_pwm_override.py do (shared harness; the flame-edge hooks stubbed as there). satSetControlMode() must be part of the cut or driven the way satCycle* drives it (check; if not reachable, say so and cover the freeze/resume through the real code paths I can compile).
- Case F1 (RED on origin/dev): flame on at t0, PWM ON phase, ON time 300 s. At t0+120 s mode -> continuous, at t0+150 s -> PWM (fail-safe), run to the end. Expect the ON phase to end at t0+300 s of PWM time, i.e. 120 s + 180 s after the re-entry (counted 150 s of continuous excluded). On origin/dev it ends 300 s after the re-entry (clock restarted): red.
- F2: three flips in one flame: the sum of the PWM segments equals the ON time, not 3 x.
- F3: flame out between two segments: the clock restarts (accumulator cleared).
- F4: ON phase ended by its timer: next phase starts at 0.
- F5: DHW draw while frozen/running: draw time not counted (characterization, passes on origin/dev too).
- F6: frozen time already >= ON time at resume: ends after the minimum-burn rule per the answer to point 3.
- F7: PWM left while waiting for ignition: no accumulated time.
- Then: evaluate --quick, full tests/, bump, esp32-combo build, bench request to otgw-bench-tester (.63: a flame phase with flips, compare PWM ON time per flame before/after).

10. DECISIONS (all recorded): entry 476 (Q-475a A: only PWM time counts, frozen in continuous, resets when the flame goes out), entry 478 (Q-475a1 A: applies to the fail-safe flips of entry 429, the only flips inside one flame), entry 479 (Q-475b A: min burn from the real flame-on). Scope 2.0.0. NOTE from entry 476: the open-loop replay of the indigo beta.410 logs showed the freeze alone would pre-empt only about 4 of 62 flips; the 448 exit and the request-above-floor effect carry the rest. Hence the three-way flip test build (dev baseline vs +448 vs +448+freeze, branch sat/flip-test).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-24 plan written, read-only on the code, nothing built or run. Waiting for Sergeant D's answer to Q-475a (BlueFlame owns the question). Branch sat/pwm-on-freeze off origin/dev is created only after the answer is recorded.

2026-11-24 plan revised after the coordinator's second read: (1) thermo-nova lines labelled NOT verified against pin 7beaaf5; fetch the pin into other-projects/SAT-releases-thermo-nova and verify pwm.py:87-97, 106-108, 134, 201, 205, plus an issue for the missing local copy, first thing after the reset. (2) entry 429 quoted in full; the fail-safe exception is explicit in option B as put to him, not in his own typed words. (3) the min-burn choice is recommendation, pending, Q-475b after Q-475a.

2026-11-25 Sergeant D decided: Q-475a A (entry 476), clarifier A (478: fail-safe flips of entry 429), Q-475b A (479: min burn from the real flame-on). AC 1 ticked on those entries. thermo-nova lines verified at the pin 7beaaf5 (fetched into other-projects/SAT-releases-thermo-nova): pwm.py:87-97, 106-108, 134, 201, 205 confirmed; the not-verified label is gone. Code next on sat/pwm-on-freeze, after TASK-3034, TASK-3008 and the Kd task (coordinator order).

2026-11-25 code on lane/d3-beta (written by a Sonnet lane in wt-d3-freeze, reviewed and integrated by otgw-dev, architect-cleared): SATcontrol.ino satPwmKeptFreeze/Clear, resume in satPwmBeginOn, clears in satPwmEndOn, satPwmDisableOnHeaterOff, satDisable and the flame-edge handler. tests/test_sat_pwm_on_freeze.py: 25 tests (F1-F10, F5a-c, F6b, F6c heat pump 600 s from the real flame-on, F11a-c, F12, source-order tests): RED on the old source 14 failures + 1 error, GREEN on the fix; mutants killed: live-flame check removed, clear before the switch, satPwmReset clearing the kept time. PWM pack (anchors, override, hp_pwm, pwm_first_start, overshoot_sustain, diag_events, no_return_ignition, ot_loss, pwm_start_limit, restart_quiet, slave_lowload, continuous_startup, disable_handback) all OK on the integrated branch. OUR READING (e), wider than entry 478's wording: the freeze applies to EVERY PWM -> continuous exit and EVERY re-entry in the same flame (the at-max exit and the 2.0 C / 60 s re-entry too, not only the fail-safe flips), because every mode change goes through satSetControlMode(). Sergeant D may object; recorded in the beta notes. Gaps (bench): real flame-edge handler, satDisable and satCycleApplyEndOfCycle ordering covered structurally; min burn never binds with the current duty mapper (ON time asked >= 180/600 s).

2026-11-25 FULL TESTS on origin/dev 686e33438 (= beta.429 build tree), after 'npm ci --prefix tests/webui': python scripts/run_tests.py --webui -j 4 --timeout 900: 475 files, 469 passed, 5 failed, 0 timeout, 1 skipped (1584.8 s). The 5 reds, none caused by this task: (1) tests/test_manual_sat_commands.py and (2) tests/test_sat_mqtt_state.py: MQTT command room_source (SATroomsource) missing in MANUAL.md and manuals; present in MQTTstuff.ino and absent from the manual already at 21e5ecb01, before beta.428 (TASK-3011/3015 lane, docs owner); (3) tests/test_sat_narrate_fits.py: SATble.ino:851 narration with an unbounded %s, from TASK-3011 (5076410fd), predates beta.428; (4) tests/test_open_questions_status.py: live queue lacks Q-466a while the table lists it; red with the older decisions pointer 71698ea too, decisions-repo bookkeeping (sat_open_questions.py --update); (5) tests/webui/settings-help.test.mjs: satroomsource help text 304 chars (limit 300), otgw-webui fixes. tests/webui/v2-undefined-names passes with acorn installed (a81749c8a). Judged against this: 'full tests green' holds except these 5 named pre-existing reds.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DRAFT 2026-11-25 (bench item pending, task NOT Done). Change (entries 475, 476, 478, 479; ADR-296 Proposed; our reading (e): the freeze covers every PWM -> continuous exit and every re-entry in one flame, wider than the wording of entry 478, Sergeant D may object): one PWM ON clock per flame. satPwmKeptFreeze() decides on the LIVE flame when PWM is left (satSetControlMode, before satPwmReset), resume in satPwmBeginOn (anchor set back by the kept time after the flame-on assignment; minimum-burn anchor = real flame-on _sat_chFlameOnMs with a fallback to the anchor of the left segment when it is 0), clear in satPwmEndOn, satPwmDisableOnHeaterOff and satDisable (after the mode switch) and in the flame-edge handler when the flame goes out; satPwmReset does NOT clear it. Evidence: tests/test_sat_pwm_on_freeze.py 25 tests (F1-F10 plus F5a-c, F6b, F6c heat pump 600 s from the real flame-on, F11a-c, F12, source-order tests): RED on the old source 14 failures + 1 error, GREEN on the fix; three mutants killed (live-flame check removed, clear before the switch, satPwmReset clearing the kept time). PWM pack OK on the integrated branch (anchors, override, hp_pwm, pwm_first_start, overshoot_sustain, diag_events, no_return_ignition, ot_loss, pwm_start_limit, restart_quiet, slave_lowload, continuous_startup, disable_handback). evaluate --quick 89/0/0, affected-tests 416 of 422 (5 pre-existing red), build OK (R = 4f303da8c). Open-loop replay (entry 476): the freeze alone pre-empts only about 4 of 62 flips; the 448 exit and the request-above-floor effect carry the rest. PENDING: AC6 bench (a flame with several flips: shorter total ON time per flame; field proof by the testers of Sergeant D); AC7 ADR-296 stays Proposed until the bench confirms. Gaps: the real flame-edge handler, satDisable and the satCycleApplyEndOfCycle ordering are covered structurally, not compiled; the minimum burn never binds with the current duty mapper (ON time asked >= 180/600 s).
<!-- SECTION:FINAL_SUMMARY:END -->
