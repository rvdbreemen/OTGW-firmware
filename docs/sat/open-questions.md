# Open questions for Sergeant D

This is the queue of SAT questions that the firmware port cannot answer by itself.
Sergeant D (George) designed SAT, so his answer decides. We ask **one question at a
time** in #alpha-testing, in plain English, with the possible answers and our recommendation.
Each answer is recorded in [`sergeant-d-corrections.md`](sergeant-d-corrections.md)
and then becomes a backlog task.

George: if a question below is unclear or wrongly put, just say so. A correction of
the question is as useful as an answer.

While George is answering, the next question follows at once. We check the chat every
hour. If a question gets no reaction within the first hour, we wait quietly until he is
back; on his next message in the channel we ask the first open question again, with a
link to the original (Robert, 2026-10-25).

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
| Q4 | answered (entry 55: option A) -> TASK-1387 | Classifier steps the firmware keeps that classifier.py does not have | TASK-1331 |
| Q5 | dropped: decided by the maintainer 2026-10-24 (opt-in setting, default off; the fallback lives in state only) -> TASK-1345 | Fallback after an MQTT outage: which units may SAT take over? (audit #2) | - |
| Q6 | answered (entry 38: curve only, no P) -> TASK-1364 | Room temperature lost or missing: stop SAT, or keep heating on curve + P as thermo-nova? Which stale time? (audit #4, entry 27, conflict C8) | - |
| Q7 | answered (entry 56: option B, stop at once) -> TASK-1390; Q7b answered (entry 57: 1 hour, option A) | Outside temperature missing or old: what should SAT use? (audit #5) | - |
| Q8 | answered (entry 40: min ON 180 s, cycle 1800/2400 s) -> TASK-1366 | Heat pumps in PWM: minimum on-time and cycles per hour (audit #9, conflicts C3/C4) | - |
| Q9 | answered (entry 41: 60 s) -> TASK-1367 | Continuous to PWM: 60 s or 3 minutes of overshoot? (audit #12, conflict C1) | - |
| Q10 | answered (entry 42: fixed 60 s) -> TASK-1368 | PID update rate: fixed 60 s as thermo-nova, or the control tick? (audit #14) | ADR-197 |
| Q11 | answered (entry 43: ON first) -> TASK-1369 | First PWM phase after PWM starts: ON at once (thermo-nova) or OFF? (audit #17) | - |
| Q12 | asked again 2026-10-25 11:36 UTC with gruijter's field data ([msg](https://discord.com/channels/812969634638725140/1514720723980259460/1563878770375397477)); note: the counter increment is pwm.py:160 (the message said :159) | Hourly start cap: count hot-water ignitions or not? (audit #18) Field evidence 2026-10-25 (gruijter alpha.500, Intergas): telnet 'cycles/hr=3' at 06:53:08; the ON at ~07:35 coincides with the boot start ageing out; the ON at ~08:35 comes one hour after the ~07:35 hot-water ignition (about 20 min of heating lost); BurnerStarts +15 against 6 visible ignitions. Homey Burner Startups (msg 1563860658905292900): +9 starts during the 07:40-07:47 UTC shower (DHW flow ~3 l/min), next heating start 08:45 UTC, one hour later. | - |
| Q13 | answered (entry 44: every update) -> TASK-1370 | CS/MM/CH refresh: every 5 s as thermo-nova, or on change + 300 s? (audit #24) | ADR-199 |
| Q14 | answered (entry 45: flow-based start, CS/PM=25 flood, MI=500) -> TASK-1371 | PWM ignition hold without a return temperature sensor (audit #27) | - |
| Q15 | answered (entry 48: a MaxTSet slider bounded by MsgID 57, sent to the boiler) -> TASK-1373 | Maximum flow setpoint: a user setting, and which defaults? (audit #29) | - |
| Q16 | answered (entry 52: default 3 gas, 2 heat pump) -> TASK-1366 + new task | Cycles per hour default and range (audit #30) | - |
| Q17 | answered (entry 53: option A) -> TASK-1376 | DHW setpoint: SW= on change, slider bounded by MsgID 48 (audit #31, entry 33) | - |
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
| Q29 | answered (entry 46: reset P, I and D to 0) -> TASK-1364 | TASK-1364 (entry 38): while the room temperature is missing, reset I and D to 0 (thermo-nova on a stale sensor) or freeze them? | TASK-1364 |
| Q30 | answered (entry 47: option B) -> TASK-1365 | TASK-1365 (entry 39): final message on lost OpenTherm: A) CH=0, CS=0 (literal), B) CS=0, CH=1, MM=T (same hand-back as switching SAT off), C) CH=0, CS=0, MM=T? | TASK-1365 |
| Q31 | answered (entry 50: option A) -> TASK-1366 | TASK-1366 (entry 40): heat pump PWM: min ON 180 s with cycle length 1800/2400 s (his answer) or 2/3 cycles per hour as thermo-nova config_flow.py:686-693? | TASK-1366 |
| Q32 | queued | TASK-1282 (entry 32): comfort humidity adjust checks the summer-off switch (bSummerSimmer, SATcontrol.ino:3460) instead of Thermal comfort. Entry 32: humidity counts only when the Summer Simmer index is on. Gate on Thermal comfort (bThermalComfort), or no gate? (gruijter 2026-10-25) | TASK-1282 |
| Q33 | asked 2026-10-25 08:25 UTC alongside Q17, on Robert's request with the urgency explained ([msg](https://discord.com/channels/812969634638725140/1514720723980259460/1563830671376384062)); asked again 2026-10-25 13:30 UTC after Q7b | TASK-1365 (entries 39, 47 Open): what counts as "no valid OpenTherm"? A) no boiler reply for 30 s, and a Data-Invalid reply counts as alive (implemented, provisional: a boiler that answers only Data-Invalid keeps SAT running); B) no boiler reply for 30 s, and Data-Invalid replies count as lost too (SAT also stops on a boiler that answers but rejects every request); C) another time window or rule (his value). Also name: OT-Direct loopback answers (GW=L) and replayed lines count as a reply too (OTGW-Core.ino:5236-5245, TASK-1138: loopback counts as boiler presence by decision); no check in simulation, and on a PIC in PS=1 summary mode a loss is not seen (no B lines); OT-Direct in PS=1 is checked. thermo-nova has no OT-loss stop (coordinator/serial/__init__.py:228-231 only on shutdown). Our advice: A | TASK-1365 |
| Q34 | queued (not yet posted; ask after Q33) | TASK-1365 (entries 39, 47 Open): when OpenTherm returns, A) SAT resumes by itself on the first boiler reply (implemented, provisional: SAT takes MsgID 1 back from the thermostat with no user action, PID integral restarts from 0); B) SAT stays stopped until the user switches it off and on (entry 39 literal 'stop working'; the thermostat keeps control, entry 26); C) resume by itself only after the link has been back for a set time. Also name: during the loss SAT stays enabled and keeps refusing external CS= (ADR-179, TASK-1236) and the OT-Direct heating curve stays disarmed; the hand-back is satDisable(), which resets and persists the PID, resets PWM and the daily curve recommendation (three LittleFS writes per loss, repeated on a flapping link; a loss at boot before the NTP-gated satLoadPidState() overwrites the saved PID with zeros; entry 28 open). Our advice: A | TASK-1365 |
| Q35 | queued (TASK-1366 review 2026-10-25) | PWM ON phase timing. The firmware times the ON end from the previous real flame-on edge (SATcontrol.ino:949-958, SATcycles.ino:722), so an ON phase whose boiler or compressor has not lit within one control tick ends on the next tick, and the 180 s ignition wait never holds; the OFF phase then restarts ON at once, so CS/CH toggle every tick. A) as thermo-nova: time ON from the phase start, wait up to 180 s for the flame, re-anchor at ignition, then start the on timer anyway (pwm.py:181-215); B) same anchor, but end the ON phase when there is no flame after 180 s (the firmware's own abort, SATcontrol.ino:959-970); C) keep today. Affects gas boilers too; since TASK-1366 a heat pump in forced PWM at duty 0.05-0.95 takes this path. Host evidence: tests/test_sat_hp_pwm.py test_r3_on_phase_waits_for_flame (expected failure). Related: entry 43 Open (first-ON conditions) and entry 45 / TASK-1371 (the 20 s ignition flood assumes the ON phase lasts through ignition; the interaction was not checked). Our advice: A (entries 8, 29) | TASK-1366 follow-up |
| Q36 | queued (TASK-1366 review 2026-10-25) | Heat pump hourly start cap. The firmware allows 2 PWM starts per rolling hour for a heat pump (SATcontrol.ino:349, SATcycles.ino:279-299); every mapped heat-pump cycle lasts at least the period, so with either period one extra flame-on in the hour (hot water, relight) holds the next PWM start. thermo-nova's cap never takes effect (pwm.py:156; duty_cycle_seconds default 00:13:00 = 780, entry_data.py:220, const.py:167). No entry sets this value (entry 30 'A' is our reading). A) no hourly cap for heat pumps, as thermo-nova; B) keep 2 per hour; C) the cap follows the period (2 per hour at 1800 s, 1.5 at 2400 s). Related to Q12. Our advice: A | - |
| Q37 | queued (TASK-1366 review 2026-10-25) | Hybrid source in PWM: cycle length and default. Today a hybrid keeps the gas timing: 180 s minimum ON, period 3600 / cycles per hour (SATcontrol.ino:364-373). Entries 50 and 52 leave hybrids open. A) gas timing (3 or 4 per hour, default 3); B) heat pump timing (1800 / 2400 s, default 1800); C) another value. thermo-nova's PWM timing has no source case at all (pwm.py:53-55; a grep of its .py files for 'hybrid' finds nothing). Our advice: A, as the smallest change from today (our reasoning, not a SAT rule) | - |
| Q38 | queued (TASK-1366 review 2026-10-25) | Minimum on time as a start gate. thermo-nova starts an ON phase only when the mapped on time is at least 180 s (pwm.py:155, HEATER_STARTUP_TIMEFRAME const.py:18); the firmware's OFF branch has no such gate (SATcontrol.ino:971-1002), so in the ultra-low case without a flame (on 0, off 2P) it still requests ON after 2P, and with Q35's timing that ON phase ends on the next tick. Entry 43 Open, audit #19; gas and heat pump alike. A) add the gate, as thermo-nova; B) no gate; C) another rule. Our advice: A (entries 8, 29) | - |
| Q39 | queued (TASK-1270, 2026-10-25) | First control tick after boot or SAT enable: the firmware PID's first call only stores the time and returns 0 (SATpid.ino ~264-285), so the first CS is 10 with CH=0 until the next update; thermo-nova's output is always curve + P + I + D (pid.py:126-131). A) send curve + P + restored I with D=0 on the first tick (as thermo-nova), B) send the curve only on the first tick, C) keep today's CS=10/CH=0 until the first full PID update. Advice A | TASK-1270 |
| Q40 | queued (boiler-capability research 2026-10-25) | Entry 45 (ignition without return temperature): how is 'no return temperature' detected? A) never reported, None as in thermo-nova (heating_control.py:393-395; coordinator/mqtt/opentherm.py:114-118); B) Unknown-DataID on MsgID 28; C) Data-Invalid or stale; D) present only through the board sensor (gruijter's Intergas, entry 6) | TASK-1371 |
| Q41 | queued (boiler-capability research 2026-10-25) | Entry 48 says the MaxTSet slider bounds are the 'MsgID 57 values (Lower/Higher)'; the spec puts the bounds in MsgID 49 and writability in MsgID 6, and some boilers misreport MsgID 6. Which applies? thermo-nova reads only MaxTSet (MsgID 57, coordinator/mqtt/opentherm.py:18) | TASK-1373 |
| Q42 | queued (boiler-capability research 2026-10-25) | Before any 'on/off boiler (MsgID 3 HB bit 1) -> PWM' mapping: does entry 13 (Force PWM removed) rule out a capability-forced PWM? | - |

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
