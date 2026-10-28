# Open questions for Sergeant D

This is the queue of SAT questions that the firmware port cannot answer by itself.
Sergeant D (George) designed SAT, so his answer decides. We ask **one question at a
time** in #alpha-testing, in plain English, with the possible answers and our recommendation.
Each answer is recorded in [`sergeant-d-corrections.md`](sergeant-d-corrections.md)
and then becomes a backlog task.

## Asking order (Robert 2026-10-26: ask interactively, prioritised)

Ask one at a time while Sergeant D is active; the next goes out as soon as one is answered.
Order: what users feel now or what blocks shipped work first, then ADR-only and detail questions.

1. Q33 (OT loss definition, TASK-1365, provisional in the field)
2. Q34 (resume after OT loss, TASK-1365)
3. Q39 (first tick after enable sends CS=10/CH=0 for ~60 s, TASK-1270)
4. Q38 (180 s minimum on time as a start gate, PWM)
5. Q44 (underheat and saturation exits: how time is counted)
6. Q32 (humidity comfort gate, TASK-1282)
7. Q20 (cold cutoff boundary, ADR-199 / TASK-1355)
8-13. Q23, Q24, Q25, Q26, Q27, Q28 (PID, ADR-197)
14-15. Q21, Q22 (heating curve, ADR-198)
Then Q48-Q51 (PWM entry and start counting vs the hourly limit, provisional decisions in force), Q35, Q36, Q37, Q40, Q41, Q42, Q43, Q45, Q46, Q47, Q18, Q19.
**Update 2026-10-28:** Sergeant D answered Q15-Q51 in batches on 2026-10-27/28 (entries 67-94). Open now, in this order: Q15b, then Q32 and Q18 (Q20b, Q28a/b, Q39b, Q46b, Q48b and Q51b were answered on 2026-10-28, entries 95-97). Q36b was a heads-up sent with the acknowledgement; Q51b is a confirmation only (entry 43 settles it) and goes last. The "Answered" column marks each answered question with its entry number (his request, entry 94). **Update 2026-10-28 evening (dependency reconciliation, TASK-1412):** open now, in this order: Q15b answered 19:06 (entry 110), then Q55 (TASK-1368, PID timer details), Q53 (TASK-1381, previous-CS clamp; blocks wave 3), Q32, Q54 (TASK-1426, heat-pump MM; needed before any heat-pump claim), Q18, then the confirmations of entries 108, 104, 107 (one per message). Q46c is withdrawn (entry 109). The numbered list above is history.

A new correction from Sergeant D (e.g. 2026-10-26 'CH=1 stays during PWM OFF') is handled before the queue.

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

| # | Answered | Status | Topic | Blocks |
|---|---|---|---|---|
| Q1 | ✓ 19 | answered (entry 19: remove it) -> TASK-1341 | Keep the firmware's "PWM auto switch" setting at all? | nothing |
| Q2 | ✓ 36 | answered (entry 36: yes, option A) -> TASK-1331 | End-of-cycle switch: compare the tail flow with the control setpoint per sample, as thermo-nova | TASK-1331 |
| Q3 | ✓ 37 | answered (entry 37: 2.0, count >= 2.0) -> TASK-1331 | Overshoot margin 2.0 equals the flow offset 2.0 | TASK-1331 |
| Q4 | ✓ 55 | answered (entry 55: option A) -> TASK-1387 | Classifier steps the firmware keeps that classifier.py does not have | TASK-1331 |
| Q5 |  | dropped: decided by the maintainer 2026-10-24 (opt-in setting, default off; the fallback lives in state only) -> TASK-1345 | Fallback after an MQTT outage: which units may SAT take over? (audit #2) | - |
| Q6 | ✓ 38 | answered (entry 38: curve only, no P) -> TASK-1364 | Room temperature lost or missing: stop SAT, or keep heating on curve + P as thermo-nova? Which stale time? (audit #4, entry 27, conflict C8) | - |
| Q7 | ✓ 56, 57 | answered (entry 56: option B, stop at once) -> TASK-1390; Q7b answered (entry 57: 1 hour, option A) | Outside temperature missing or old: what should SAT use? (audit #5) | - |
| Q8 | ✓ 40 | answered (entry 40: min ON 180 s, cycle 1800/2400 s) -> TASK-1366 | Heat pumps in PWM: minimum on-time and cycles per hour (audit #9, conflicts C3/C4) | - |
| Q9 | ✓ 41 | answered (entry 41: 60 s) -> TASK-1367 | Continuous to PWM: 60 s or 3 minutes of overshoot? (audit #12, conflict C1) | - |
| Q10 | ✓ 42 | answered (entry 42: fixed 60 s) -> TASK-1368 | PID update rate: fixed 60 s as thermo-nova, or the control tick? (audit #14) | ADR-197 |
| Q11 | ✓ 43 | answered (entry 43: ON first) -> TASK-1369 | First PWM phase after PWM starts: ON at once (thermo-nova) or OFF? (audit #17) | - |
| Q12 | ✓ 59, 58 | answered (entry 59: option A, confirmed after entry 58) -> TASK-1401 | Hourly start cap: count hot-water ignitions or not? (audit #18) Field evidence 2026-10-25 (gruijter alpha.500, Intergas): telnet 'cycles/hr=3' at 06:53:08; the ON at ~07:35 coincides with the boot start ageing out; the ON at ~08:35 comes one hour after the ~07:35 hot-water ignition (about 20 min of heating lost); BurnerStarts +15 against 6 visible ignitions. Homey Burner Startups (msg 1563860658905292900): +9 starts during the 07:40-07:47 UTC shower (DHW flow ~3 l/min), next heating start 08:45 UTC, one hour later. gruijter graph 2026-10-25 evening (msg 1563977137898262579): 8 flame-on edges, 7 coincide with DHW (clusters of 3), 1 CH-only (5 s sampling). | - |
| Q13 | ✓ 44 | answered (entry 44: every update) -> TASK-1370 | CS/MM/CH refresh: every 5 s as thermo-nova, or on change + 300 s? (audit #24) | ADR-199 |
| Q14 | ✓ 45 | answered (entry 45: flow-based start, CS/PM=25 flood, MI=500) -> TASK-1371 | PWM ignition hold without a return temperature sensor (audit #27) | - |
| Q15 | ✓ 67 | answered (entries 48, 67: defaults 62 radiators / 55 underfloor) -> TASK-1373; follow-up Q15b | Maximum flow setpoint: a user setting, and which defaults? (audit #29) | - |
| Q16 | ✓ 99 | answered (entries 52, 99: gas 3 or 4 only, default 3) -> TASK-1375 | Cycles per hour default and range (audit #30) | - |
| Q17 | ✓ 69 | answered (entries 53, 69: yes, re-confirmed) -> TASK-1376 | DHW setpoint: SW= on change, slider bounded by MsgID 48 (audit #31, entry 33) | - |
| Q18 |  | queued | Zones: follow area.py (max, no headroom, cap overshoot)? (audit #32, entry 35) | - |
| Q19 | ✓ 70 | answered (entry 70: leave them as is) -> no change (TASK-1347) | Units whose coefficient was set by the old wizard (1.4 / 0.6 / 0.8, not chosen by the user): leave them (entry 24) or reset them once to 2.0? (TASK-1347) | - |
| Q20 | ✓ 98 | answered (entries 71, 98: at or below the cold setpoint) -> TASK-1355 | Cold cutoff boundary: is a requested setpoint exactly equal to the cold setpoint (28.2 / 21) cold (`<=`, as thermo-nova heating_control.py:72 and :385) or not (`<`)? Entries 7 and 11 left it open (TASK-1355) | ADR-199 |
| Q21 | ✓ 72 | answered (entry 72: 5.0 is fine) -> ADR-198 | Heating curve coefficient upper bound: is 5.0 enough, or should it go to 12 as thermo-nova config_flow.py:616-618? (ADR-198, TASK-1359) | ADR-198 |
| Q22 | ✓ 72 | answered (entry 72: remove the auto-tune) -> TASK-1413 | The firmware auto-tunes the coefficient hourly (SATcontrol.ino:4664-4705), thermo-nova does not: keep or remove? (ADR-198, audit #39) | ADR-198 |
| Q23 | ✓ 73 | answered (entry 73: follow thermo-nova) -> TASK-1414, ADR-197 | PID resets: reset integral and derivative on every target, HVAC-mode and window change as thermo-nova, or keep the firmware's narrower resets? (ADR-197, audit #15) | ADR-197 |
| Q24 | ✓ 74 | answered (entry 74: follow thermo-nova) -> TASK-1415, ADR-197 | PID persistence: save and restore integral, last error, raw derivative, last temperature and the derivative time as thermo-nova, or only integral and derivative after a 30 min age check? (ADR-197, audit #42) | ADR-197 |
| Q25 | ✓ 75 | answered (entry 75: follow thermo-nova, remove cap and 10 s minimum) -> TASK-1416, ADR-197 | Firmware-only integral cap of plus/minus 20 and 10 s minimum between PID updates: keep or remove? (ADR-197, entry 28 asked 2026-06-21, no answer) | ADR-197 |
| Q26 | ✓ 76 | answered (entry 76: fixed 0.1) -> TASK-1417, ADR-197 | Deadband as a user setting (0.05-2.0) and its widening during room-temperature estimation: keep, or a constant 0.1 as thermo-nova const.py:16? (ADR-197, audit #38) | ADR-197 |
| Q27 | ✓ 77 | answered (entry 77: match thermo-nova) -> TASK-1418, ADR-197 | Derivative details: 'no change' threshold 0.001 vs exactly 0, and the reference temperature that moves only with the derivative time (thermo-nova pid.py:171, :211): match thermo-nova? (ADR-197, audit #38/#40) | ADR-197 |
| Q28 | ✓ 78, 95 | answered (entry 78; scope Q28a and Ki clamp Q28b answered in entry 95) -> TASK-1423 | Integral with Ki near 0 skips the clamp (SATpid.ino:166) and no rounding of gains/terms/output (pid.py:71-132): match thermo-nova? (ADR-197, audit #38) | ADR-197 |
| Q29 | ✓ 46 | answered (entry 46: reset P, I and D to 0) -> TASK-1364 | TASK-1364 (entry 38): while the room temperature is missing, reset I and D to 0 (thermo-nova on a stale sensor) or freeze them? | TASK-1364 |
| Q30 | ✓ 47 | answered (entry 47: option B) -> TASK-1365 | TASK-1365 (entry 39): final message on lost OpenTherm: A) CH=0, CS=0 (literal), B) CS=0, CH=1, MM=T (same hand-back as switching SAT off), C) CH=0, CS=0, MM=T? | TASK-1365 |
| Q31 | ✓ 50 | answered (entry 50: option A) -> TASK-1366 | TASK-1366 (entry 40): heat pump PWM: min ON 180 s with cycle length 1800/2400 s (his answer) or 2/3 cycles per hour as thermo-nova config_flow.py:686-693? | TASK-1366 |
| Q32 |  | queued | TASK-1282 (entry 32): comfort humidity adjust checks the summer-off switch (bSummerSimmer, SATcontrol.ino:3460) instead of Thermal comfort. Entry 32: humidity counts only when the Summer Simmer index is on. Gate on Thermal comfort (bThermalComfort), or no gate? (gruijter 2026-10-25) | TASK-1282 |
| Q33 | ✓ (maintainer) | answered by the maintainer: A (Robert, 2026-10-26 19:13 UTC, "@BlueFlame Bot Q33: A", [msg](https://discord.com/channels/812969634638725140/1514720723980259460/1564356412585287824)); asked of Sergeant D 2026-10-25 08:25, 13:30, 19:58 and 2026-10-26 18:39 UTC without an answer -> TASK-1365 | TASK-1365 (entries 39, 47 Open): what counts as "no valid OpenTherm"? A) no boiler reply for 30 s, and a Data-Invalid reply counts as alive (implemented, provisional: a boiler that answers only Data-Invalid keeps SAT running); B) no boiler reply for 30 s, and Data-Invalid replies count as lost too (SAT also stops on a boiler that answers but rejects every request); C) another time window or rule (his value). Also name: OT-Direct loopback answers (GW=L) and replayed lines count as a reply too (OTGW-Core.ino:5236-5245, TASK-1138: loopback counts as boiler presence by decision); no check in simulation, and on a PIC in PS=1 summary mode a loss is not seen (no B lines); OT-Direct in PS=1 is checked. thermo-nova has no OT-loss stop (coordinator/serial/__init__.py:228-231 only on shutdown). Our advice: A | TASK-1365 |
| Q34 | ✓ 79 | answered (entry 79: A, resume on the first boiler reply) -> TASK-1365 | TASK-1365 (entries 39, 47 Open): when OpenTherm returns, A) SAT resumes by itself on the first boiler reply (implemented, provisional: SAT takes MsgID 1 back from the thermostat with no user action, PID integral restarts from 0); B) SAT stays stopped until the user switches it off and on (entry 39 literal 'stop working'; the thermostat keeps control, entry 26); C) resume by itself only after the link has been back for a set time. Also name: during the loss SAT stays enabled and keeps refusing external CS= (ADR-179, TASK-1236) and the OT-Direct heating curve stays disarmed; the hand-back is satDisable(), which resets and persists the PID, resets PWM and the daily curve recommendation (three LittleFS writes per loss, repeated on a flapping link; a loss at boot before the NTP-gated satLoadPidState() overwrites the saved PID with zeros; entry 28 open). Our advice: A | TASK-1365 |
| Q35 | ✓ 80 | answered (entry 80: A) -> TASK-1382 | PWM ON phase timing. The firmware times the ON end from the previous real flame-on edge (SATcontrol.ino:949-958, SATcycles.ino:722), so an ON phase whose boiler or compressor has not lit within one control tick ends on the next tick, and the 180 s ignition wait never holds; the OFF phase then restarts ON at once, so CS/CH toggle every tick. A) as thermo-nova: time ON from the phase start, wait up to 180 s for the flame, re-anchor at ignition, then start the on timer anyway (pwm.py:181-215); B) same anchor, but end the ON phase when there is no flame after 180 s (the firmware's own abort, SATcontrol.ino:959-970); C) keep today. Affects gas boilers too; since TASK-1366 a heat pump in forced PWM at duty 0.05-0.95 takes this path. Host evidence: tests/test_sat_hp_pwm.py test_r3_on_phase_waits_for_flame (expected failure). Related: entry 43 Open (first-ON conditions) and entry 45 / TASK-1371 (the 20 s ignition flood assumes the ON phase lasts through ignition; the interaction was not checked). Our advice: A (entries 8, 29) | TASK-1366 follow-up |
| Q36 | ✓ 81 | answered (entry 81: A, no hourly cap for heat pumps; narrows entry 59) -> TASK-1419; heads-up Q36b | Heat pump hourly start cap. The firmware allows 2 PWM starts per rolling hour for a heat pump (SATcontrol.ino:349, SATcycles.ino:279-299); every mapped heat-pump cycle lasts at least the period, so with either period one extra flame-on in the hour (hot water, relight) holds the next PWM start. thermo-nova's cap never takes effect (pwm.py:156; duty_cycle_seconds default 00:13:00 = 780, entry_data.py:220, const.py:167). No entry sets this value (entry 30 'A' is our reading). A) no hourly cap for heat pumps, as thermo-nova; B) keep 2 per hour; C) the cap follows the period (2 per hour at 1800 s, 1.5 at 2400 s). Related to Q12. Our advice: A | - |
| Q37 | ✓ 82 | answered (entry 82: A, gas timing) -> TASK-1375 | Hybrid source in PWM: cycle length and default. Today a hybrid keeps the gas timing: 180 s minimum ON, period 3600 / cycles per hour (SATcontrol.ino:364-373). Entries 50 and 52 leave hybrids open. A) gas timing (3 or 4 per hour, default 3); B) heat pump timing (1800 / 2400 s, default 1800); C) another value. thermo-nova's PWM timing has no source case at all (pwm.py:53-55; a grep of its .py files for 'hybrid' finds nothing). Our advice: A, as the smallest change from today (our reasoning, not a SAT rule) | - |
| Q38 | ✓ 80 | answered (entry 80: A, 180 s start gate) -> TASK-1383 | Minimum on time as a start gate. thermo-nova starts an ON phase only when the mapped on time is at least 180 s (pwm.py:155, HEATER_STARTUP_TIMEFRAME const.py:18); the firmware's OFF branch has no such gate (SATcontrol.ino:971-1002), so in the ultra-low case without a flame (on 0, off 2P) it still requests ON after 2P, and with Q35's timing that ON phase ends on the next tick. Entry 43 Open, audit #19; gas and heat pump alike. A) add the gate, as thermo-nova; B) no gate; C) another rule. Our advice: A (entries 8, 29) | - |
| Q39 | ✓ 83 | answered (entry 83: A) -> TASK-1270; D term on the first tick asked as Q39b | First control tick after boot or SAT enable: the firmware PID's first call only stores the time and returns 0 (SATpid.ino ~264-285), so the first CS is 10 with CH=0 until the next update; thermo-nova's output is always curve + P + I + D (pid.py:126-131). A) send curve + P + restored I with D=0 on the first tick (as thermo-nova), B) send the curve only on the first tick, C) keep today's CS=10/CH=0 until the first full PID update. Advice A | TASK-1270 |
| Q40 | ✓ 84 | answered provisionally (entry 84: B "for now, we may need to reconsider later") -> TASK-1371 | Entry 45 (ignition without return temperature): how is 'no return temperature' detected? A) never reported, None as in thermo-nova (heating_control.py:393-395; coordinator/mqtt/opentherm.py:114-118); B) Unknown-DataID on MsgID 28; C) Data-Invalid or stale; D) present only through the board sensor (gruijter's Intergas, entry 6) | TASK-1371 |
| Q41 | ✓ 100 | answered (entry 100: A, MsgID 49 bounds with 20/80 fallback, always send SH=, SAT caps CS) -> TASK-1373 | Entry 48 says the MaxTSet slider bounds are the 'MsgID 57 values (Lower/Higher)'; the spec puts the bounds in MsgID 49 and writability in MsgID 6, and some boilers misreport MsgID 6. Which applies? thermo-nova reads only MaxTSet (MsgID 57, coordinator/mqtt/opentherm.py:18) | TASK-1373 |
| Q42 | ✓ 102 | answered (entry 102: A, SAT ignores the on/off bit) -> no task | Before any 'on/off boiler (MsgID 3 HB bit 1) -> PWM' mapping: does entry 13 (Force PWM removed) rule out a capability-forced PWM? | - |
| Q43 | ✓ 86 | answered (entry 86: A, >=) -> TASK-1420 | 60 s overshoot switch, the boundary. Entry 37 ('2 or more') decided `>=` for the end-of-cycle classifier; the 60 s switch still needs flow strictly above requested + margin (SATcycles.ino:993). thermo-nova counts equality too: it resets only on `flow < requested + margin` (heating_control.py:288-291). A) `>=`, as thermo-nova and as entry 37 for the classifier; B) keep `>`. The practical difference is a flow exactly on the threshold. Entry 41 Open lists it. Our advice: A | TASK-1367 follow-up |
| Q44 | ✓ 86 | answered (entry 86: A) -> TASK-1420 | Underheat and saturation exits (PWM -> continuous): how is the time counted? Entry 41 covered only the overshoot switch. thermo-nova counts both from the first qualifying sample with `sustained_runtime` (heating_control.py:342-345, :364-367; helpers.py:51-56), 180 s and 300 s (const.py:22-23), and counts underheat while `flow <= requested - 3` (reset on `>`, heating_control.py:337-340; cycles/const.py:11). The firmware adds the time since the previous call, the first call included, so at a 30 s tick underheat fires after about 150 s and saturation after about 270 s; above a 60 s tick neither fires (SATcycles.ino:1014-1022); underheat uses a strict `<` (SATcycles.ino:1032). A) count both as thermo-nova (first sample starts the timer, no 60 s bail-out, `<=` for underheat); B) keep today; C) as A but keep the strict `<`. Our advice: A | TASK-1367 follow-up |
| Q45 | ✓ 87 | answered (entry 87: A, after Q35) -> TASK-1419 | Heat pumps and the 60 s overshoot switch. The firmware never runs the switch for a heat pump (gate `!satAlwaysMaxModulation()`, SATcontrol.ino:5313; heat pump test SATcontrol.ino:376-380), so a heat pump never enters PWM by itself. thermo-nova's switch has no heat-pump case (heating_control.py:262-305), and entries 40 and 50 run heat pumps in PWM. A) remove the exclusion, as thermo-nova (heat-pump PWM timing from entries 40/50 applies; Q35 and Q36 are still open for heat-pump PWM); B) keep it (heat pumps stay continuous unless the user picks PWM); C) another rule. Our advice: A, after Q35 | TASK-1367 follow-up |
| Q46 | ✓ 86 | answered (entry 86: B, scheduled tick time) -> TASK-1420; scope Q46b | 60 s switch and the control tick. (Premise corrected in entry 109: the timer skips missed ticks, ticks are never closer than one interval.) Ticks run on a fixed grid (CATCH_UP_MISSED_TICKS, SATcontrol.ino:590; safeTimers.h:192-193), but the time is read a few ms late in each tick, so about half the time the tick 60 s after the start measures 59 99x ms and the switch comes one tick later: 90 s at the 30 s default, 120 s at a 60 s tick (derived from the code, not measured). thermo-nova has the same strict test but a 5 s loop (climate.py:706-709), so it lands at 60-65 s. A) keep the strict millis() test (60 s or one tick later); B) time on the tick's scheduled time, so the switch comes on the first tick at least 60 s after the start; C) allow a small tolerance (for example 1 s). Our advice: B | TASK-1367 follow-up |
| Q47 | ✓ 88 | answered (entry 88: A) -> TASK-1420 | 60 s switch: same tick or next tick? The firmware computes that tick's continuous CS (SATcontrol.ino:5297) and only then runs the switch (SATcontrol.ino:5314), so the first PWM CS goes out one control tick later (30 s at the default). thermo-nova enables PWM before it computes the setpoint in the same update (heating_control.py:206-228). A) run the switch before the CS, as thermo-nova; B) keep today. Not new in TASK-1367. Our advice: A | TASK-1367 follow-up |
| Q48 | ✓ 89 | answered (entry 89: A, but a hot-water flame never counts) -> TASK-1421; bit asked as Q48b | Entries 22/43 keep the burner on when PWM is entered, entry 59 makes cycles per hour a real limit. When the PWM-entry ON is the 4th start of the hour: A) it counts but the limit never cuts it (provisional, Robert 2026-10-26; thermo-nova counts it, pwm.py:160, and its cap at pwm.py:156 never blocks at defaults), B) it counts and the limit cuts the burner, C) it does not count. | - |
| Q49 | ✓ 90 | answered (entry 90: B, count at the ON start; only after TASK-1382) -> TASK-1421 | When does a PWM ON phase count against cycles per hour? A) at the first flame of the phase, so a failed ignition never uses up the limit (firmware today). B) at the ON start, as thermo-nova (pwm.py:160): every ON start counts, also one that never lights; with today's on/off flap on slow ignition (Q35) three restarts in about 90 s would block PWM for up to an hour. C) something else. Our advice: A until Q35 is fixed, then B | TASK-1401 |
| Q50 | ✓ 91 | answered (entry 91: A) -> TASK-1412.9 (confirms TASK-1401) | When an ON phase starts with the flame already burning (also in duty range 5, and when PWM is entered), what does its ON time count from? A) from that start, as thermo-nova (pwm.py:162-167, :205-209). B) from the last real ignition: a burner that is kept burning is then cut on the next tick. Our advice: A | TASK-1401, TASK-1369 |
| Q51 | ✓ 91 | answered (entry 91: A) -> TASK-1380; scope Q51b | If the flame is still lit at the first tick after ON to OFF, does the off wait count from the ON-to-OFF edge? A) yes, as thermo-nova (pwm.py:208). B) no, from the last real flame-off: the burner restarts at once and uses up an hourly start (about 30% fewer real heat starts under a one-tick lag in the review probe). Our advice: A | TASK-1401 |
| Q52 | ✓ 103 (A, B, C) | answered A, B, C (entry 103) -> TASK-1422; D expand requested, asked as Q52D | Entry 95 (pause the PWM ON timer during a draw), three details: A) after the draw SAT waits up to 180 s (timed from the draw end) for the CH flame, then resumes the ON timer anyway (as entry 80); B) the resume continues the remaining ON time with no new 180 s hold and no new boiler-temperature seed (hot DHW flow), like a relight (pwm.py:144-146); C) a flame still burning at the first tick after the draw resumes at once; D) (entry 101) an ON start at the first tick after a draw with the hot-water flame still lit counts as a normal start (pwm.py:160). Our advice: A, B, C, D | TASK-1422, TASK-1421 |
| Q52D | ✓ 105 | answered "No" (entry 105): such a start does not count; B or C asked as Q52E | Entry 101 / Q52 D expanded: does an ON start right after a draw, with the hot-water flame still lit, count against the hourly limit? | TASK-1421 |
| Q52E | ✓ 106 | answered (entry 106: C, no ON start while the hot-water flame burns) -> TASK-1424 | After Q52D "No": B) the start happens but is not counted (SAT remembers the DHW-lit flame), or C) no ON start while that hot-water flame burns, start after it goes out (counted)? | TASK-1424 |
| Q15b | ✓ 110 | answered (entry 110: B, keep the 50 ceiling, underfloor default 50) -> TASK-1373 | MaxTSet default for underfloor: 55 against the firmware's 50 hard ceiling and 45 system cap; and the heat-pump default | TASK-1373 |
| Q46c | | withdrawn (entry 109: the control timer skips missed ticks, so no catch-up burst exists) | Was: catch-up grid replaying missed ticks back to back. Wrong premise: initSAT re-sets timerSATControl to SKIP_MISSED_TICKS (SATcontrol.ino:4277, safeTimers.h:96). | TASK-1420 |
| Q55 | ✓ 111 | answered (entry 111: A, full thermo-nova; no room temperature = curve only, entries 38/46) -> TASK-1368 | 60 s PID timer (entry 42): two details. (1) Zones: do area PIDs also move to their own 60 s timer, as area.py:183-190? (2) Does the 60 s step also run while the control tick skips (hot water: thermo-nova keeps stepping, its control_pid is its own timer; firmware-only skips: boiler fault, thermal-safe fallback, summer simmer, valves closed)? A) full thermo-nova: zones too, and the step runs on its own timer in all of these, only SAT off / OT loss / no room temperature stop it; B) zones too, but the step pauses whenever the tick skips (today's firmware behaviour for the integral); C) main PID only, zones stay per tick, step as A. Our advice: A. | TASK-1368 |
| Q53 | | asked 2026-10-28 19:49 UTC (msg 1565090124415639564) | Continuous mode, previous-CS term in the flow clamp. thermo-nova: CS = max(req, flow-2) when the request rises above the previous CS, otherwise max(req, min(flow-2, previous CS)) (heating_control.py:485-489; previous = its last CS, :454). Firmware: only max(req, flow-2) (SATcontrol.ino:1603-1610), so CS follows a rising flow upward. A) follow thermo-nova, previous = the last CS SAT sent (10 after heater-off/cold cutoff, 0 after enable/boot); B) keep today; C) as A and restore previous after a reboot (heating_control.py:186-187). Our advice: A. Firmware-only sub-point (our choice, recorded with the answer): thermal-safe and summer simmer write the CS they send as previous. | TASK-1381 |
| Q54 | | queued, draft (TASK-1426, entry 21 Open; citations to be opened before posting) | Heat pump modulation (MM) in PWM: today a heat pump gets the configured maximum in PWM and 100 outside PWM (TASK-1419 review). Which value should a heat pump get, and is it the same in and outside PWM? | TASK-1426 |
| Q20b | ✓ 98 | answered (entry 98: at or below, `<=`, corrects entry 96) -> TASK-1355 | Cold cutoff: is a requested setpoint exactly equal to 28.2 / 21 cold (<=, thermo-nova) or not (<)? | TASK-1355, ADR-199 |
| Q28a | ✓ 95 | answered (entry 95: all PID values 3 decimals) -> TASK-1423 | "Round PID values to 3 decimals": which values (gains, P/I/D terms, output)? | ADR-197 |
| Q28b | ✓ 95 | answered (entry 95: match thermo-nova) -> TASK-1423 | Ki near 0 skips the integral clamp: match thermo-nova? | ADR-197 |
| Q36b | | heads-up, not blocking (entry 81) | The hourly counter now counts only PWM ON starts (TASK-1401), so the Q36 premise changed | - |
| Q39b | ✓ 95 | answered (entry 95: D = 0) -> TASK-1270 | First tick after enable: D = 0, or the restored raw derivative? | TASK-1270 |
| Q46b | ✓ 95 | answered (entry 95: yes) -> TASK-1420 | Does timing on the scheduled tick time also apply to the other sustained timers? | TASK-1412.8 |
| Q48b | ✓ 95 | answered (entry 95: boiler DHW active; pause the ON timer during a draw) -> TASK-1421, TASK-1422 | Which DHW bit (boiler DHW active or thermostat DHW enable), and does a hot-water flame that continues into a PWM ON phase count? | TASK-1412.9 |
| Q51b | ✓ 97 | answered (entry 97: follow thermo-nova) -> TASK-1380 | Always time the PWM OFF wait from the ON-to-OFF switch, also when the flame is already off (entry 92 report)? | - |

Questions Q5 onwards come from the SAT port audit of 2026-10-24
([`sat-port-audit-2026-10-24.md`](sat-port-audit-2026-10-24.md)); the numbers in brackets are its
finding numbers.

Order: by audit priority. Q2-Q4 come first because TASK-1331 waits on them. The low-priority audit
findings marked "SD?" are asked after these. Each question gets its full text, possible answers and
our advice (with the thermo-nova citations opened again) only when it is asked.

## Q1. Keep the "PWM auto switch" setting?

**Context.** The firmware has a setting "PWM auto switch" (on by default,
`SATtypes.h` `bPwmAutoSwitch`). When it is off, SAT does not use the 60-second
overshoot switch into PWM (`SATcycles.ino` `satCycleCheckOvershootSwitch`; thermo-nova
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
