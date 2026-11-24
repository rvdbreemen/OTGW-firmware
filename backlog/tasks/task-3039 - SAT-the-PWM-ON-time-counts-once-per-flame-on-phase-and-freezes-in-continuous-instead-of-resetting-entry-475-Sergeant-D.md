---
id: TASK-3039
title: >-
  SAT: the PWM ON time counts once per flame-on phase and freezes in continuous
  instead of resetting (entry 475, Sergeant D)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-24T18:27:00Z'
updated_date: '2026-11-24T18:32:00Z'
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
- [ ] #1 Sergeant D has answered Q-475a and the answer is recorded as an entry in the decisions register before any code
- [ ] #2 Host test: a scripted flame-on phase with 3 PWM/continuous flips is red on origin/dev (the ON timer restarts at each re-entry) and green on the fix (the ON timer freezes and resumes)
- [ ] #3 Host test: the clock resets when the flame goes out, when an ON phase ends normally, and when SAT is disabled or the heater goes off; DHW draw time still does not count (entry 95 unchanged)
- [ ] #4 The start gate (entries 385/387, ADR-252), the 448/456 exit rule, the at-max rule (entry 463) and the entry 428/429 per-flame counting are unchanged (existing tests green)
- [ ] #5 evaluate.py --quick shows no new FAIL; full tests/ run green; esp32-combo build OK
- [ ] #6 Bench on .63 (bench-tester): a flame-on phase with several PWM/continuous flips shows a shorter total ON time per flame than before; field proof by Sergeant D's testers
- [ ] #7 ADR (adr-kit, Proposed) records the freeze rule and cites the entry
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
STATUS: PLAN ONLY. No code before Sergeant D answers Q-475a. All firmware citations are origin/dev 9f0ccba38 (src/OTGW-firmware, SATcontrol.ino = C, SATcycles.ino = Y), opened in this session. Read-only; nothing built or tested.

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
- The 180 s minimum burn (C:1797, pwm.py:201): measured from _pwm_flameOnMs, which satPwmBeginOn sets to now at the re-entry. If it stays per segment, a resume with little ON time left still burns 180 s more, which weakens the intent. Recommendation, PENDING (design choice, becomes Q-475b to Sergeant D after he answers Q-475a; BlueFlame asks): count the minimum burn from the real flame-on of the flame (_sat_chFlameOnMs, C:8123), not per segment. This is Q-475a point 3.
- Heat pump (minOnMs 600 s, C:1796-1797): same rule.
- Start gate / 448 exit: unchanged. A flip needs the exit rule to fire, which is gated for 60 s after the first CH flame-on (ADR-252).
- Telemetry (TASK-1880, satPwmPhaseOpen/Close, C:1441, :1495): stays per PWM segment; add the used time to the open record only if cheap.
- MM=T / at-max (entry 463) and the flame-off hold: unaffected, they read bPwmFlameRequested.

7. THERMO-NOVA - NOT VERIFIED AGAINST THE PIN 7beaaf5. other-projects/SAT-releases-thermo-nova/ does not exist in this checkout (only other-projects/SAT-develop, HEAD 2b01ba65, which is not the pin). I read a copy in another clone (OTGW-other-projects/SAT-releases-thermo-nova, its own .git HEAD 770aa45, not the pin; the pin object is not in it). The lines below match what entry 475 cites, but they are unverified. Morning fix: fetch commit 7beaaf5 (github.com/Alexwijn/SAT, branch releases/thermo-nova) into other-projects/SAT-releases-thermo-nova, verify pwm.py:87-97, 106-108, 134, 201, 205 there, and file an issue for the missing local copy (coordinator order, 2026-11-24).
- pwm.py:106-108 disable() calls reset(); reset() (:87-97) sets _last_update = timestamp() and clears _flame_on_at. :134 elapsed = timestamp - self._last_update. :205 if elapsed >= on_time_seconds: ON ends. :201 the flame-on hold from _flame_on_at.
- PWM and continuous are chosen outside this class (heating_control.py), so thermo-nova clears the ON clock whenever PWM is left. DIFFERENCE: our change keeps the clock across PWM stints within one flame. By design a deviation from the pin (higher source: entry 475).

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

10. OPEN FOR SERGEANT D (Q-475a, BlueFlame owns it): A only PWM time counts (recommended: continuous time does not eat the ON time, matches "freeze") / B all flame time counts / C other; plus point 3: does the 180 s minimum burn run per segment or from the real flame-on. Scope is 2.0.0 per Robert.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-24 plan written, read-only on the code, nothing built or run. Waiting for Sergeant D's answer to Q-475a (BlueFlame owns the question). Branch sat/pwm-on-freeze off origin/dev is created only after the answer is recorded.

2026-11-24 plan revised after the coordinator's second read: (1) thermo-nova lines labelled NOT verified against pin 7beaaf5; fetch the pin into other-projects/SAT-releases-thermo-nova and verify pwm.py:87-97, 106-108, 134, 201, 205, plus an issue for the missing local copy, first thing after the reset. (2) entry 429 quoted in full; the fail-safe exception is explicit in option B as put to him, not in his own typed words. (3) the min-burn choice is recommendation, pending, Q-475b after Q-475a.
<!-- SECTION:NOTES:END -->
