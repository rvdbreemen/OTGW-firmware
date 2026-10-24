# Open questions for Sergeant D

This is the queue of SAT questions that the firmware port cannot answer by itself.
Sergeant D (George) designed SAT, so his answer decides. We ask **one question at a
time** in #alpha-testing, in plain English, with the possible answers and our recommendation.
Each answer is recorded in [`sergeant-d-corrections.md`](sergeant-d-corrections.md)
and then becomes a backlog task.

George: if a question below is unclear or wrongly put, just say so. A correction of
the question is as useful as an answer.

We check for an answer every hour and stay quiet while there is none. When a question
has waited 12 hours and 3 or more other messages have pushed it out of view, we repeat
it once, with a link to the original.

On 2026-10-24 at 20:58 UTC Sergeant D got the full list Q2-Q28 as an overview
([part 1](https://discord.com/channels/812969634638725140/1514720723980259460/1563656700118958262),
[part 2](https://discord.com/channels/812969634638725140/1514720723980259460/1563656702086221987));
he may answer any of them out of order ("Q12: ..."). The detailed questions still come one at a time.

Status values: **queued** (not asked yet), **asked** (waiting for the answer),
**answered** (recorded as an entry), **dropped** (no longer needed, with the reason).

| # | Status | Topic | Blocks |
|---|---|---|---|
| Q1 | answered (entry 19: remove it) -> TASK-1341 | Keep the firmware's "PWM auto switch" setting at all? | nothing |
| Q2 | answered (entry 36: yes, option A) -> TASK-1331 | End-of-cycle switch: compare the tail flow with the control setpoint per sample, as thermo-nova | TASK-1331 |
| Q3 | answered (entry 37: 2.0, count >= 2.0) -> TASK-1331 | Overshoot margin 2.0 equals the flow offset 2.0 | TASK-1331 |
| Q4 | asked again with more detail 2026-10-24 21:31 UTC ([msg](https://discord.com/channels/812969634638725140/1514720723980259460/1563666347832119430)) | Classifier steps the firmware keeps that classifier.py does not have | TASK-1331 |
| Q5 | dropped: decided by the maintainer 2026-10-24 (opt-in setting, default off; the fallback lives in state only) -> TASK-1345 | Fallback after an MQTT outage: which units may SAT take over? (audit #2) | - |
| Q6 | answered (entry 38: curve only, no P) -> TASK-1364 | Room temperature lost or missing: stop SAT, or keep heating on curve + P as thermo-nova? Which stale time? (audit #4, entry 27, conflict C8) | - |
| Q7 | clarification asked 2026-10-24 21:31 UTC ([msg](https://discord.com/channels/812969634638725140/1514720723980259460/1563666350818463826)); his OT-loss rule is entry 39 -> TASK-1365 | Outside temperature missing or old: what should SAT use? (audit #5) | - |
| Q8 | answered (entry 40: min ON 180 s, cycle 1800/2400 s) -> TASK-1366 | Heat pumps in PWM: minimum on-time and cycles per hour (audit #9, conflicts C3/C4) | - |
| Q9 | answered (entry 41: 60 s) -> TASK-1367 | Continuous to PWM: 60 s or 3 minutes of overshoot? (audit #12, conflict C1) | - |
| Q10 | answered (entry 42: fixed 60 s) -> TASK-1368 | PID update rate: fixed 60 s as thermo-nova, or the control tick? (audit #14) | ADR-197 |
| Q11 | answered (entry 43: ON first) -> TASK-1369 | First PWM phase after PWM starts: ON at once (thermo-nova) or OFF? (audit #17) | - |
| Q12 | asked again with more detail 2026-10-24 21:31 UTC ([msg](https://discord.com/channels/812969634638725140/1514720723980259460/1563666349450985620)) | Hourly start cap: count hot-water ignitions or not? (audit #18) | - |
| Q13 | answered (entry 44: every update) -> TASK-1370 | CS/MM/CH refresh: every 5 s as thermo-nova, or on change + 300 s? (audit #24) | ADR-199 |
| Q14 | queued | PWM ignition hold without a return temperature sensor (audit #27) | - |
| Q15 | queued | Maximum flow setpoint: a user setting, and which defaults? (audit #29) | - |
| Q16 | queued | Cycles per hour default and range (audit #30) | - |
| Q17 | queued | DHW setpoint: SW= on change, slider bounded by MsgID 48 (audit #31, entry 33) | - |
| Q18 | queued | Zones: follow area.py (max, no headroom, cap overshoot)? (audit #32, entry 35) | - |
| Q19 | queued | Units whose coefficient was set by the old wizard (1.4 / 0.6 / 0.8, not chosen by the user): leave them (entry 24) or reset them once to 2.0? (TASK-1347) | - |
| Q20 | queued | Cold cutoff boundary: is a requested setpoint exactly equal to the cold setpoint (28.2 / 21) cold (`<=`, as thermo-nova heating_control.py:72 and :385) or not (`<`)? Entries 7 and 11 left it open (TASK-1355) | ADR-199 |
| Q21 | queued | Heating curve coefficient upper bound: is 5.0 enough, or should it go to 12 as thermo-nova config_flow.py:616-618? (ADR-198, TASK-1359) | ADR-198 |
| Q22 | queued | The firmware auto-tunes the coefficient hourly (SATcontrol.ino:4664-4705), thermo-nova does not: keep or remove? (ADR-198, audit #39) | ADR-198 |
| Q23 | queued | PID resets: reset integral and derivative on every target, HVAC-mode and window change as thermo-nova, or keep the firmware's narrower resets? (ADR-197, audit #15) | ADR-197 |
| Q24 | queued | PID persistence: save and restore integral, last error, raw derivative, last temperature and the derivative time as thermo-nova, or only integral and derivative after a 30 min age check? (ADR-197, audit #42) | ADR-197 |
| Q25 | queued | Firmware-only integral cap of plus/minus 20 and 10 s minimum between PID updates: keep or remove? (ADR-197, entry 28 asked 2026-06-21, no answer) | ADR-197 |
| Q26 | queued | Deadband as a user setting (0.05-2.0) and its widening during room-temperature estimation: keep, or a constant 0.1 as thermo-nova const.py:16? (ADR-197, audit #38) | ADR-197 |
| Q27 | queued | Derivative details: 'no change' threshold 0.001 vs exactly 0, and the reference temperature that moves only with the derivative time (thermo-nova pid.py:171, :211): match thermo-nova? (ADR-197, audit #38/#40) | ADR-197 |
| Q28 | queued | Integral with Ki near 0 skips the clamp (SATpid.ino:166) and no rounding of gains/terms/output (pid.py:71-132): match thermo-nova? (ADR-197, audit #38) | ADR-197 |

Questions Q5 onwards come from the SAT port audit of 2026-10-24
([`sat-port-audit-2026-10-24.md`](sat-port-audit-2026-10-24.md)); the numbers in brackets are its
finding numbers.

Order: by audit priority. Q2-Q4 come first because TASK-1331 waits on them. The low-priority audit
findings marked "SD?" are asked after these. Each question gets its full text, possible answers and
our advice (with the thermo-nova citations opened again) only when it is asked.

## Q1. Keep the "PWM auto switch" setting?

**Context.** The firmware has a setting "PWM auto switch" (on by default,
`SATtypes.h` `bPwmAutoSwitch`). When it is off, SAT does not use the 60-second
overshoot switch into PWM (`SATcycles.ino` `satCycleCheckAutoSwitch`; thermo-nova
`heating_control.py:280-304`). thermo-nova has no such setting. Entry 18 already
says the new end-of-cycle switch ignores it.

**Proposals.**
- A. Remove the setting, as we did with Force PWM (entry 13). SAT always picks the
  mode itself, exactly as thermo-nova.
- B. Keep it as an expert option for the 60-second switch only.

## Q2. End-of-cycle switch: per-sample control setpoint

**Context.** thermo-nova classifies a cycle by comparing each tail sample's flow
with that sample's own control setpoint (`cycles/tracker.py:267-271`,
`sample.control_setpoint`). Our classifier uses one control setpoint taken when the flame came on.
Once the classification switches the mode (entry 15), this matters: if SAT leaves
PWM in the middle of a burn, the flow is compared with the old, low PWM setpoint and
the cycle looks like an overshoot, so SAT goes straight back to PWM at flame-off.
thermo-nova does not do that.

**Proposals.**
- A. Store the control setpoint per tail sample, as thermo-nova (about 720 bytes of
  memory), and only then switch modes at the end of a cycle. (Recommended.)
- B. Ship the end-of-cycle switch with the per-cycle setpoint and accept the
  ping-pong risk.
- C. Keep the end-of-cycle switch off until A is done.

## Q3. Overshoot margin 2.0 and the flow offset 2.0

**Context.** You chose margin 2.0 for the end-of-cycle overshoot (entry 15;
thermo-nova uses 3.0, `cycles/const.py:10`). In continuous mode thermo-nova keeps
the control setpoint at least at the boiler temperature minus the flow offset, 2.0
by default (`heating_control.py:485-489`, `const.py:154`), so a clamped sample has an error of
at most 2.0, below thermo-nova's 3.0. With margin 2.0 and "2.0 or more counts", a
cycle whose tail sits on that clamp would count as an overshoot and switch to PWM.

**Proposals.**
- A. Keep 2.0 but count only "more than 2.0".
- B. Use 3.0, as thermo-nova.
- C. Keep 2.0 and "2.0 or more" (accept that clamped cycles switch to PWM).

## Q4. Classifier steps that classifier.py does not have

**Context.** Entry 15 asked us to align the cycle classifier with `classifier.py`
first. Our classifier still has steps that thermo-nova does not have: a "short
cycle" result for continuous cycles (after the overshoot test), "shorter than
120 s = uncertain", and no "uncertain" result when the cycle kind is unknown or
percentiles are missing (`classifier.py:22-23`, `:42-43`, `:71-72`). Other parts of
the firmware read the "short cycle" result (flame status, cycle health).

**Proposals.**
- A. Remove all of them and match `classifier.py` exactly.
- B. Keep the continuous "short cycle" result (used for display only) and remove the
  rest.
- C. Keep them as they are.
