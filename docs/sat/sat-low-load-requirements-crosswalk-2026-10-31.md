# SAT low-load requirements crosswalk (2026-10-31)

> **Errata after the independent second read (2026-11-01, `sat-low-load-requirements-crosswalk-2026-10-31.second-read.md`, 92 rows: 69 confirmed, 3 wrong, 17 imprecise, 3 unverifiable).** Every thermo-nova vs firmware difference below held up. Corrections that override the text:
> 1. Section 5 summary: no input has all five measurement states. Outside temperature folds Unknown-DataID and Data-Invalid into "no fresh Read-Ack" (OTGW-Core.ino:5677-5681).
> 2. Section 8: item 2 (three separate reply states for ID3/25/28) is a new contract derived from the spec, not recorded or implemented behaviour.
> 3. Section 8 item 3: not every transmitted CS is capped by the effective ceiling. The thermal-safe fallback caps with satGetMaxSetpoint() (user MaxTSet only, SATcontrol.ino:6515-6522) instead of satEffectiveMaxSetpoint() (:405), and the 5 s resend (:634-641) and the PIC keepalive (:597-606) repeat the last CS without re-clamping after a limit change. Filed as its own task.
> The 17 imprecise rows (wording, line numbers, a missed second PWM entry path pwm.on_cycle_end in thermo-nova, the REST/MQTT control_mode path to manual PWM) are listed with corrections in the second-read file; read both together.

Deliverable of **TASK-1412.2** (review coverage R01, R06, R07, R08, R09, R14, R15, R17, R25, R41 of the [remediation plan](sat-low-load-remediation-plan-2026-10-28.md)).

**What this document is.** A map from each OpenTherm clause and each recorded SAT decision to what the firmware does today, the contract an implementation task must meet, the test that proves it, and the approval it needs. It decides nothing. Open policy goes to section 9 ("Queued for Sergeant D") as proposed questions. It does not edit an ADR, a task, the question queue or the decision log.

**What this document is not.** It is not evidence that anything works on a boiler. No build, flash or bench run was done for it. Host tests that ran in this session are listed in section 10 with their command and counts; a host test is not a physical result.

## 0. Frozen references

| Item | Value |
|---|---|
| Firmware revision | `dev` at `c2ed654ec01f7dee134ff150c5c8b0a742c7d0f6` (checkout of 2026-10-31/11-01). `git diff --stat HEAD` over `src/OTGW-firmware/SAT*` and `OTGW-Core.ino` was empty: the cited sources were clean at that commit. Line numbers below are at this commit; they also hold at `3fc03a369` (only version banners changed, section 10). |
| thermo-nova | `releases/thermo-nova` @ `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`, local copy `other-projects/SAT-releases-thermo-nova/`, paths relative to `custom_components/sat`. Only the ranges listed in section 11 were opened in this session; any other thermo-nova location is marked "(cited in entry N, not re-opened)". |
| OpenTherm protocol | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md`. Cited as section, PDF page (the `## Pagina N` marker in the file) and line in the repository text. |
| OpenTherm application spec (AFS) | OpenTherm Application Functional Specification v1.1f, `D:/Users/Robert/Documents/GitHub/RvdB/OTGW-other-projects/OT-Spec/` (outside this repository). Section text read from the `.md` there; printed page numbers taken from the PDF text layer in the adjacent docling `.json` (field `page`, matching the printed "Page N" footer). The PDF itself could not be rendered in this session (no `pdftoppm`), so figures were not checked. |
| Decision log | [sergeant-d-corrections.md](sergeant-d-corrections.md), entries 1 to 190 as on 2026-10-31. |
| Planning documents | The 2026-10-28/29 plans describe an older state. Since then TASK-1365, 1370, 1371, 1373, 1375, 1380-1383, 1385, 1390, 1405, 1513, 1531 are Done. Every "firmware today" statement below comes from code opened at the revision above, not from the register's wording. |

Second-reader status: CLAUDE.md requires a second reader to open both citations before a thermo-nova-versus-firmware difference is published. **Every difference claim in this document is pending that second read.**

## 1. Classes

| Class | Meaning | Who can change it |
|---|---|---|
| **M** | Mandatory OpenTherm requirement: the text says "must", "mandatory" or defines a bit/reply meaning. | Nobody in this project; a deviation is an interoperability defect. |
| **R** | Suggested low-load recipe or recommendation (AFS sections headed "Suggestion" or "recommendation", protocol "should"). Not mandatory, even where the sentence says "has to". | Sergeant D for SAT behaviour; Robert for firmware-only behaviour. |
| **D** | Recorded SAT decision: a numbered entry in the decision log (Sergeant D, or the maintainer where the entry says so). | Only a later recorded entry. |
| **P** | New proposal in this document or an open plan item. Needs approval before implementation. | As named per item. |

ADR-196 (Accepted) orders SAT ground truth (decisions, then thermo-nova, then specs, then firmware) and states in its Scope that "The OpenTherm specification and the other reference ladders in CLAUDE.md ... are unchanged" (`docs/adr/ADR-196-sat-behaviour-ground-truth-sergeant-d-s-recorded-decisions-then-pinned-thermo-nova-then-specs-then-firmware.md:160-163`). So an M item is not overruled by thermo-nova parity or by entry 29 ("Python always wins"). This document applies that existing boundary; it does not create a new one.

## 2. OpenTherm clauses used in this crosswalk

| Id | Class | Source | What it says (paraphrase, key words quoted) |
|---|---|---|---|
| OT-M1 | M | AFS 2.1.1.2.10, p9 | Slave config ID3 HB bit 4 is the master low-load control function. bit=0: allowed, "The master must switch it on". bit=1: disallowed, "The master must switch it off". "It's mandatory for the master to read ID3 at start-up and check bit 4. In case of 'Unknown DataID' response from the slave the master must use his own Low-Load control function." |
| OT-M2 | M | Protocol 5.3.2, p36, line 1610 | ID3 HB bit 4: "Master low-off&pump control function [allowed, not allowed]" (1 = not allowed). |
| OT-M3 | M | Protocol 5.2.1, p32, lines 1342-1360 | Master must send ID0 READ_DATA, ID1 WRITE_DATA (INVALID_DATA "not recommended"), ID3 READ_DATA "at least at start up"; slave must answer ID3 with READ_ACK. |
| OT-M4 | M | Protocol 4.4.1 and 4.4.2, p29, lines 1200-1236 | Three distinct replies: READ-ACK (valid value), DATA-INVALID ("recognised ... but the data requested is not available or invalid"), UNKNOWN-DATAID ("does not recognise"). A Data-Invalid reply is not an Unknown-DataID reply. |
| OT-M5 | M | Protocol 5.2.1, p32, lines 1365-1373 | ID17 and ID25 are not mandatory; the slave answers READ_ACK or DATA_INVALID ("for example if sensor fault"). A master must therefore be able to receive Data-Invalid for flow and modulation. What the master then does is not specified. |
| OT-M6 | M | Protocol 5.3.1, p36, lines 1580-1581 | "The CHenable bit has priority over the Control Setpoint." |
| OT-M7 | M | Protocol 5.3.1, p34, lines 1432 and 1444 | ID0 HB bit 0 is master "CH enable"; ID0 LB bit 1 is slave "CH mode"; LB bit 3 is flame status. Different bits, different owners. |
| OT-DEF1 | definition | AFS 3.2, p25 | Relative modulation 0% is the boiler's minimum modulation level, not off. A definition used to read MM=0, not a requirement on SAT. |
| OT-R1 | R | AFS 2.1.1.2.1, p6 | Suggested low-load detection, all of: flame off while heat requested; last ID17 not above 5% (if present); ID25 above ID1; not when ID25 is at maximum (ID57); not shortly after DHW; not when ID3 HB4 is set; not shortly after boiler start-up. Remark: a fast setpoint change "Maybe ... is not a real low-load situation". |
| OT-R2 | R | AFS 2.1.1.2.2, p6 | On detection switch to capacity (on/off) control with on and off times from the room deviation. |
| OT-R3 | R | AFS 2.1.1.2.3, p6 | OFF period: switch the boiler off completely by clearing ID0 HB bit 0; ID1 "could be set to a low value". |
| OT-R4 | R | AFS 2.1.1.2.4, p6 | Start in low load: ID1 to maximum (ID57), ID14 to 0% if present, CH enable, wait for CH mode and flame, wait 10-20 s, then ID1 1 K below boiler temperature, repeated every 5 s. |
| OT-R5 | R | AFS 2.1.1.2.5, p7 | ON period without ID14: raise ID1 until flame, then ID1 = boiler temperature or 1 K lower, updated "in short time intervals of 2 - 3 seconds". |
| OT-R6 | R | AFS 2.1.1.2.6, p7 | ON period with ID14: CH enable, ID14 0%, ID1 maximum. |
| OT-R7 | R | AFS 2.1.1.2.7, p7 | Back to modulating when ON time reaches 100%: first ID1 to the normal value, then ID14 back to 100%. |
| OT-R8 | R | AFS boiler minimum off-time section, p10 | Boiler anti-cycling off times degrade fixed-cycle low-load control; suggests correcting the cycle with the measured flame delay. |
| OT-R9 | R | AFS 2.2.3, p17 | After a DHW interruption, master recommendation: with CH demand keep the boiler on by setting ID1 equal to the boiler temperature; after 1 minute without a falling boiler temperature restore the setpoint; if the boiler switches off, stay modulating. |
| OT-R10 | R | AFS 3.1, p24 | Master: accept ID0 after two equal receptions; "Sent all write messages regularly." |
| OT-R11 | R | Protocol 5.3.5, p42, lines 2015-2026 | Master "should read" remote-parameter bounds (ID49 for max CH setpoint); without them it "can apply default bounds as it chooses". ID6 flags "indicate" transfer-enable and read/write. There is no "must not write" tied to ID6. |

## 3. Crosswalk per review item

Each item lists: clause and class; thermo-nova; firmware today; recorded decisions; the contract for the implementing task; the test; the approval.

### R01: ID3 HB4 permission is ignored by SAT

- **Clause.** OT-M1, OT-M2 (M). Detection condition "not when HB4 is set" in OT-R1 (R) repeats it.
- **thermo-nova.** Exposes the bit only as a binary sensor (`coordinator/serial/binary_sensor.py:54`); a grep of `custom_components/sat` for `low_off`/`pump_control` finds no control use. The sustained-overshoot entry (`heating_control.py:262-305`) has no HB4 check.
- **Firmware today.** The bit is decoded only for MQTT (`OTGW-Core.ino:3186`). The topic `supports_pump_control` reads ON when bit 4 = 1, which means "not allowed": the new name reads inverted (relevant for TASK-1412.15). SAT reads `SlaveConfigMemberIDcode` only for the DHW-tank flag (`SATcontrol.ino:3806`); no SAT code tests bit 4 (0x1000). PWM entry paths that a veto would have to cover:
  1. sustained overshoot switch, `SATcycles.ino:1103-1131`, called at `SATcontrol.ino:6689-6693`;
  2. boot restore of "PWM enabled", `SATcontrol.ino:5090-5091` (entry 121);
  3. manual mode command `pwm`/2, `SATcontrol.ino:3647-3649` (API kept by our decision, entry 10);
  4. the end-of-cycle switch, not implemented (TASK-1331 To Do).
  ID3 is requested at SAT boot (`PM=3`, comment at `SATcontrol.ino:5095`) and polled every 60 s on OT-Direct (`OTDirect.ino:58`, `:162`). On a PIC gateway, who reads ID3 and how often was **not traced**.
- **Decisions.** Entry 179 (maintainer, 2026-10-31): bit 4 = 1 means no SAT PWM, SAT stays modulating; build after Sergeant D's OK. Entry 102 (HB bit 1 on/off type does not force PWM) is a different bit and unaffected.
- **Contract (TASK-1412.4).** The meaning is fixed by M and is not a question: bit 4 = 1 from a valid ID3 Read-Ack forbids master low-load control; an explicit Unknown-DataID on ID3 means SAT uses its own PWM. Implementation of the veto still waits for Sergeant D's OK, because entry 179 (Robert) recorded that it is built only after that OK. **Blocked** as policy: which entry paths a veto covers (QD-1), what happens when the bit flips during PWM (QD-2), and what "not yet known" means (QD-3). Representation required whatever the answers: allowed / disallowed / Unknown-DataID / not yet read this boot / last reply Data-Invalid, each a separate state with the time of the last valid reply.
- **Test.** Missing: to be written in TASK-1412.3 (LL-010).
- **Approval.** Sergeant D (entry 179, ADR-196: thermo-nova does not do this). His OK covers the whole veto implementation; the M meaning is presented to him as fixed by the specification, and the questions ask only for the policy choices.

### R06: overshoot entry is a temperature proxy, not proof of minimum-power saturation

- **Clause.** OT-R1, OT-R2 (R). No M clause defines when a master must enter low load (only OT-M1, when it must not).
- **thermo-nova.** `heating_control.py:262-305`: flow >= requested + margin for 60 s, skipped while PWM is on, during hot water, within `DHW_OVERSHOOT_GUARD_SECONDS` (300, `const.py:19`) after it, with heater off, requested <= cold, or flow None. Margin 3.0 (`cycles/const.py:10`), 60 s (`const.py:21`). Request at or above the maximum disables PWM (`heating_control.py:203-204`).
- **Firmware today.** `SATcycles.ino:1103-1131`: `flow >= requested + fOvershootMargin` (default 2.0), 60 s from the first qualifying tick, cleared outside continuous mode and within the DHW guard (`SATcycles.ino:175-179`). The caller skips it at or below the cold setpoint (`SATcontrol.ino:6689-6693`). Request at the system maximum forces continuous (`SATcontrol.ino:6676-6680`). Differences with thermo-nova: the firmware switch has no flow-validity reset (TASK-1428 To Do); margin 2.0 is decided (entries 15, 22, 37). Compared with OT-R1, neither implementation uses ID17 (<= 5%), flame-off-while-requested, or a "shortly after boiler start-up" exclusion; a cold start runs continuous first (entry 22), which covers part of the start-up exclusion.
- **Decisions.** Entries 22, 41, 86 (60 s, >=, tick time), 87 (heat pumps enter the same way), 88/134 (order in the tick), 108/131 (DHW guard).
- **Contract (TASK-1412.7).** Keep the recorded entry rule. Record it in the code and docs as an anticipatory heuristic: it does not prove the boiler sits at minimum output. The OT-R1 conditions that are not adopted are listed as differences, not as defects. Adding an ID17 or flame-off condition is a P item that needs Sergeant D and field evidence first; none is proposed here.
- **Test.** Exists (run, section 10): `tests/test_sat_overshoot_sustain.py::test_dhw_guard_blocks_overshoot_for_300s_after_the_draw`, `::test_dhw_guard_overshoot_exact_window`, `::test_dhw_guard_inactive_without_a_draw_near_the_millis_wrap`, `::test_heat_pump_overshoot_switches_to_pwm`. Missing (TASK-1412.3, LL-012): setpoint fall, maximum-temperature limiting, pump step, start-up high fire, flow None.
- **Approval.** None to keep today's rule. Sergeant D for any change.

### R07: switching overshoot protection off also removes PWM OFF actuation

- **Clause.** OT-R3 (R): OFF period clears CH enable. OT-M6 (M): CH enable outranks the setpoint, so the OFF phase is only effective with CH=0.
- **thermo-nova.** `heating_control.py:385`: with `overshoot_protection` false the PWM override returns, so the OFF phase does not set `MINIMUM_SETPOINT` (`:388-389`). CH follows `_control_setpoint > COLD_SETPOINT` (`:238`), MM is 0 while PWM is enabled (`:139-140`). With protection off the OFF phase keeps the previous CS and, for a CS above 22, CH=1.
- **Firmware today.** Same result. `satApplyPWM` returns the previous CS when protection is off (`SATcontrol.ino:1373-1375`); `satPwmDrivesSetpoint` is then false (`:1358-1362`), so `satChEnableDecide` gives CH=1 in the OFF phase while the request is above the cold setpoint (`SATchEnable.h:46-51`). The 3 s refresh and the no-return flood are off too (`SATcontrol.ino:1406`, `:1528`). MM is 0 in PWM for a gas boiler (`SATcontrol.ino:467-479`). Net effect: the PWM phases run as bookkeeping; the boiler is never switched off by SAT.
- **Decisions.** Entries 5, 9, 17: PWM OFF sends CS=10 with CH=0. Entry 20: the override is a setting, on by default, the user can switch it off. Entry 11: our question 1 asked exactly this case (protection off in PWM, held CS 24: keep CH on or off?); our reading of his answer 1 (TASK-1323 notes) is CH=1, because CH follows the requested setpoint. ADR-199 (Accepted 2026-10-29) records that reading as a deliberate departure: "overshoot protection off, held CS 24, requested setpoint 30 gives CH=1 in the firmware. That this follows from entry 11 answer 1 is our reading" (`docs/adr/ADR-199-sat-cold-state-and-ch-enable-rule-supersedes-adr-195.md:133`). So the CH bit with protection off rests on our interpretation of entry 11, recorded in an Accepted ADR. What has never been put to him directly is whether the OFF phase should still switch the boiler off with protection off (entries 5/17 against entry 20).
- **Contract (TASK-1412.6).** **Blocked: QD-4** (a confirm-or-revise question on our entry 11 reading). Until answered, today's behaviour stays and is documented as "protection off = no OFF-phase actuation". TASK-1412.6 must not change CH, cold-cutoff or ownership semantics (its AC#3). If he chooses B or C, the CH rule recorded in ADR-199 changes too, which needs a superseding ADR (Robert holds lifecycle authority) as well as his answer.
- **Test.** Exists (run, section 10): `tests/test_sat_pwm_override.py::test_protection_off_keeps_previous_cs_in_off_phase`, `::test_start_tick_protection_off_keeps_previous_cs`, `::test_protection_off_has_no_suppression_override`, `::test_refresh_idle_with_protection_off`; `tests/test_sat_pwm_anchors.py::test_k3_without_overshoot_protection_there_is_no_hold`. These pin today's behaviour; they do not pin an approved contract. Missing (LL-017): toggling the setting during continuous, ON, OFF, ignition wait and recovery.
- **Approval.** Sergeant D for the behaviour; Robert for a superseding ADR if B or C is chosen.

### R08: return temperature absent, invalid or stale

- **Clause.** OT-M4 (M): Unknown-DataID, Data-Invalid and a valid value are different replies. No clause defines an age. Using the return temperature for the ignition hold is SAT design, not an OT clause.
- **thermo-nova.** Only `None` versus a value (`coordinator/mqtt/opentherm.py:114-118`, base class `coordinator/__init__.py:188-191`). With `None` the flame-off hold returns and keeps the previous CS (`heating_control.py:392-395`). `MAX_BOILER_TEMPERATURE_AGE = 60` is defined (`const.py:40`) and used nowhere (grep this session).
- **Firmware today.** Partly fixed since the register was written (TASK-1371 Done). `satNoReturnTemp()` (`SATcontrol.ino:2278-2282`) is true only when: MsgID 28 is marked Unknown-DataID (`OTGW-Core.ino:5202`), a boiler frame of this boot confirmed it (`OTGW-Core.ino:5198`), and no valid board sensor 'R' reading exists (`OTGW-Core.ino:1145-1159`; validity 5 min, `:1117-1124`). That boiler type gets flow + offset as hold (`SATcontrol.ino:970-978`) and the CS/PM=25 flood (`:1546-1580`). Not represented: a Data-Invalid reply leaves `Tret` at its last value, because the store happens only for a valid reply (`OTGW-Core.ino:1900-1909`, `:2831`); a value that stops arriving is used forever; 0.0 is the never-received sentinel (`OTGW-Core.h:83`) and is floored to the cold setpoint by the hold (`SATcontrol.ino:974`). The "this boot" rule and the 5 min sensor validity are our implementation choices, marked so in the code comments (`SATcontrol.ino:2268-2277`).
- **Decisions.** Entry 45 (flood design), 84 (Q40 B "for now": Unknown-DataID), 135 (Q61 A: a valid 'R' sensor counts), 141 (MI=500 rule), 142, 149, 151. Open: entry 84 Open #1 (OT-Direct `boilerEvidence` not traced) and #3 (Data-Invalid/stale, his "reconsider later"); entry 135 Open (maximum sensor age, fallback for '-').
- **Contract (TASK-1412.5, TASK-1412.12).** Unknown-DataID path: as implemented (D). Data-Invalid and stale return: **blocked: QD-5**. The "this boot" and 5 min choices: **blocked as confirmation: Conf-1371**. Whatever the answers, the return input carries separate states (section 5), and a 0.0 sentinel is never read as a measured 0 °C.
- **Test.** Exists (run, section 10): `tests/test_sat_no_return_ignition.py::test_hold_is_flow_plus_offset_without_return_temperature`, `::test_flood_alternates_cs_and_pm25_each_second_for_20_s_then_suppression`, `::test_a_restored_unknown_dataid_verdict_alone_is_no_return_nothing`, `::test_valid_board_sensor_counts_as_a_return_temperature`, `::test_normal_boiler_is_unchanged` and the MI=500 tests at lines 334-372. Missing (LL-011, LL-018): Data-Invalid on 28, a return that stops arriving, recovery, OT-Direct verdict formation.
- **Approval.** Sergeant D (QD-5, Conf-1371).

### R09: the 3 s refresh can follow a stale flow temperature indefinitely

- **Clause.** OT-R5 (R): update ID1 "with the latest boiler temperature" every 2-3 s. OT-M5 (M): ID25 may be answered Data-Invalid. No clause defines a maximum sample age.
- **thermo-nova.** Flow `None` sets PWM to IDLE (`pwm.py:122-128`); the sustained switches reset on `None` (`heating_control.py:284-286`); the continuous clamp skips on `None` (`:458-465`). No age check (`const.py:40` unused).
- **Firmware today.** `satGetFlowTemp()` returns `OTcurrentSystemState.Tboiler` with no age, source or validity (`SATcontrol.ino:2256-2260`). Never received is the 0.0 sentinel (`OTGW-Core.h:80`); PWM treats `<= 0` as no flow and sends the continuous CS (`SATcontrol.ino:1369-1371`), the continuous clamp skips `<= 0` or `> 100` (`:2019-2025`). A Data-Invalid reply leaves the last value (`OTGW-Core.ino:1900-1909`, `:2831`). The PS=1 summary also writes `Tboiler` with no age (`OTGW-Core.ino:4638`). The 3 s refresh (`SATcontrol.ino:1398-1422`) recomputes flow - offset from that value. On OT-Direct ID25 is polled every 10 s (`OTDirect.ino:57`, `:146`), so the 3 s refresh reuses one sample for up to about 10 s by design; how stale it can get on a PIC gateway was **not traced**. The sustained switches have no flow-validity reset (TASK-1428 To Do).
- **Decisions.** Entry 23 (~3 s refresh), 44 (send every update), 109 (control timer skips missed ticks). No entry decides a flow age or a Data-Invalid response.
- **Contract (TASK-1412.5, TASK-1428, TASK-1412.11).** **Blocked: QD-6** for the fallback. Not blocked, can be specified now: record per-sample receive time and source for ID25 so that sample age at CS computation can be measured (verification plan section 5 target "<= 3 s" is a P engineering target, not an OT requirement, and still needs approval before a bench run).
- **Test.** Exists (run, section 10): `tests/test_sat_pwm_override.py::test_no_flow_temperature_idles_pwm_and_sends_the_continuous_setpoint` (covers only the 0.0 sentinel). Missing (LL-011): frozen positive flow, Data-Invalid on 25, late and reordered frames, recovery.
- **Approval.** Sergeant D (QD-6). Robert for the transport latency budget (TASK-1412.11).

### R14: heat-pump PWM entry and timing

- **Clause.** OT-DEF1: MM=0 is minimum output for a boiler; the AFS gives no heat-pump rule. No M clause covers compressor protection.
- **thermo-nova.** No heat-pump case in the sustained switch (`heating_control.py:262-305`). Period and options per heat pump: cited in entries 30, 40, 184 (`pwm.py:54`, `config_flow.py:686-693`), not re-opened here.
- **Firmware today.** Heat pump and hybrid period from `iHpCycleSeconds` 1800 or 2400 s (`SATcontrol.ino:453-460`); minimum ON 180 s for every source (`:446-450`); gas and hybrid hourly start limit 3 or 4 (`:436-442`; heat pumps exempt per the comment, entry 81; the limiter itself not re-opened). Heat pumps enter PWM through the sustained switch (`SATcycles.ino:1103-1131`, entry 87). MM: heat pump 100 outside PWM, configured maximum in PWM, never 0 (`SATcontrol.ino:467-479`). Entry 113 says a heat pump always gets the configured maximum, also outside PWM: the firmware does not follow it yet (TASK-1426 To Do, implementation gap, not a question).
- **Decisions.** Entries 21, 30, 40, 50, 52, 81, 82 (hybrid gas timing, narrowed by 186/188), 87, 113, 155 (40 °C default), 181/182 (maintainer: fewest compressor starts first, heat pumps may deviate from thermo-nova), 184 (study of a 7200 s option), 186/188 (hybrid takes only the heat-pump cycle).
- **Contract (TASK-1412.17, TASK-1527, TASK-1426).** Gas and hybrid: today's rules (D). Heat pump: today's rules stay in force (entries 40, 50, 87 until Sergeant D revises them, as entry 182 itself says); the replacement is **blocked: Q-1527a** (already asked, not re-queued). No heat-pump "compressor protection" claim is allowed while equipment limits are unknown (TASK-1412.17 AC#2).
- **Test.** Exists (run, section 10): `tests/test_sat_hp_pwm.py` (`test_hp1800_*`, `test_hp2400_*`, `test_hybrid_uses_the_heat_pump_cycle`, `test_cph_getter_heat_pump_period_ignores_cph`), `tests/test_sat_pwm_start_limit.py::test_hp_has_no_hourly_start_limit`, `tests/test_sat_pwm_override.py::test_heat_pump_and_hybrid_never_zero`. Missing: entry 113 outside-PWM MM (TASK-1426), full-loop automatic entry per source (LL-015).
- **Approval.** Sergeant D (Q-1527a). Robert (entries 181/182 are his) for the objective.

### R15: MaxTSet IDs and the all-path ceiling

- **Clause.** OT-R11 (R): read ID49 bounds, else default bounds; ID6 flags indicate support and writability. No M clause forbids writing ID57 when ID6 says read-only; the slave answers with Data-Invalid or Unknown-DataID (OT-M4). The handoff's "entry 100 versus ID6" flag is therefore an interoperability choice, not an M violation. This document does not resolve it.
- **thermo-nova.** Cited in entries 100 and 152 (`number.py:116-127`, `const.py:36`, `config_flow.py:612-613`), not re-opened here.
- **Firmware today.** Bounds from ID49, 20/80 fallback (`SATcontrol.ino:314-319`); boiler value: user value, else 62/50 default, cut to bounds and the hard system ceiling, global 65 only for an unchosen value (`:330-347`); heat-pump 40 °C default cap (`:349-358`); `SH=` only after the user chose a value, once per change or restart, no ID6 check, no retry (`:375-395`); CS ceiling also includes the boiler's own `MaxTSet` (`:403-415`). All CS paths opened here are capped: tick final clamp (`SATcontrol.ino:6727-6728`), PWM hold and suppression (`:970-987`), 3 s refresh (`:1418-1420`), flood (`:1557-1560`, `:1571`). SAT never reads the write verdict `isBoilerMsgIdUnsupportedWrite(57)` (`OTGW-Core.ino:5205`; grep of `SAT*.ino` finds only the read bitmap).
- **Decisions.** Entries 48, 51, 54, 67, 100 (ID49 bounds, ID57 value, no ID6 gate), 110, 148 (no SH= until chosen), 152 (no global cap above a chosen value), 155.
- **Contract (TASK-1412.13).** As decided (D). Observing and publishing a rejected `SH=` write is a P item: **QR-1** (Robert).
- **Test.** Exists (run, section 10): `tests/test_sat_max_tset.py` (all tests at lines 170-317, e.g. `test_not_chosen_radiators_default_62_and_no_sh`, `test_invalid_bounds_fall_back_to_20_80`, `test_heat_pump_not_chosen_sends_nothing_cs_40`); `tests/test_sat_no_return_ignition.py::test_hold_is_clamped_to_the_user_max`. Missing (LL-014): lowering the limit during an ON phase, rejected write, recovery path.
- **Approval.** None for the decided parts. Robert for QR-1.

### R17: the device profile must not follow an unreliable source-detection hint

- **Clause.** No OT clause identifies a heat source. ID3 HB bit 2 (cooling supported) is a capability bit, not a source class (protocol line 1610 area, OT-M2 table).
- **thermo-nova.** One axis, `HeatingSystem` (cited in entry 30, `types.py:110-118`, not re-opened). No source detection.
- **Firmware today.** `satGetEffectiveHeatingSource()` returns the manual setting; AUTO resolves to gas boiler (`SATcontrol.ino:297-302`). The detected source is telemetry only (comment at `:290-296`, ADR-157 Decision). HB bit 1 (on/off type) does not force PWM (entry 102).
- **Decisions.** ADR-157 (Accepted, maintainer, prompted by Sergeant D's question). Entry 30 proposed auto-detection from ID3 LB2 "to be confirmed" by Robert; ADR-157 chose manual configuration instead. Entry 186/188 settle hybrid scope.
- **Contract (TASK-1412.4 AC#2, TASK-1412.17).** Manual source is authoritative; AUTO = gas; HB1, HB2, HB4, ID14 support and manufacturer quirks are separate inputs; HB4 (R01) may veto PWM but never changes the source.
- **Test.** Missing: no test asserts that AUTO resolves to gas or that `iDetectedHeatingSource` never reaches control (grep of `tests/*.py` for `iDetectedHeatingSource`/`heating_source_detected` finds nothing). To be written in TASK-1412.3.
- **Approval.** None (ADR-157 governs).

### R25: post-DHW guard versus the AFS recovery recommendation

- **Clause.** OT-R9 (R, AFS 2.2.3 p17), OT-R1 "not shortly after DHW" (R).
- **thermo-nova.** 300 s guard on the sustained switches (`heating_control.py:268-274`, `:313-315`, `const.py:19`). No post-DHW setpoint hold found in the ranges opened; the continuous clamp keeps CS >= flow - offset with the flame on (`heating_control.py:485-489`), which partly does what OT-R9 suggests.
- **Firmware today.** During DHW the control tick returns early and sends nothing (`SATcontrol.ino:6420-6446`); the PWM ON clock is frozen and resumed (`:6447-6452`, entries 95, 103, 132, 158); the 300 s guard (`SATcycles.ino:175-179`, `:211-216`); continuous clamp with previous-CS term (`SATcontrol.ino:2002-2035`, entry 112). No 1-minute CS = boiler-temperature hold as in OT-R9.
- **Decisions.** Entries 92, 95, 103, 105, 106, 108, 131, 132, 133, 158.
- **Contract (TASK-1412.7, TASK-1429).** Keep the recorded behaviour. TASK-1412.7 AC#2 must produce the side-by-side comparison with OT-R9 from traces. Any added hold is a P item: **QD-8**, to be asked only after that comparison exists.
- **Test.** Exists (run, section 10): `tests/test_sat_overshoot_sustain.py::test_dhw_resets_underheat_and_blocks_it_for_300s`, `::test_dhw_resets_saturation_and_blocks_it_for_300s`, `::test_dhw_return_notes_the_dhw_tick`. Missing (LL-012): sub-tick draws (TASK-1429), the OT-R9 comparison trace, master CH-enable versus slave CH-mode as separate signals.
- **Approval.** None to keep today. Sergeant D for QD-8.

### R41: OT-requirement-to-test mapping and release proof

- **Clause.** All of section 2.
- **Contract.** This document is the clause-to-behaviour-to-test map. TASK-1412.3 turns its "missing" entries into executable contracts in `scripts/verify_sat_low_load.py`. Release proof stays the staged sequence of the handoff (1412.18 to 1412.26).
- **LL gate coverage today** (existing tests are partial unless stated; none is a whole-loop test):

| Gate | Existing partial coverage | Missing (TASK-1412.3) | Blocked by |
|---|---|---|---|
| LL-010 HB4/ID14 | none | everything | QD-1, QD-2, QD-3 |
| LL-011 measurement validity | 0.0 flow sentinel, Unknown-DataID return | Data-Invalid, stale, frozen, recovery, per-ID age | QD-5, QD-6 |
| LL-012 entry and DHW | DHW guard, heat-pump entry | setpoint fall, max limiting, pump step, CH-enable vs CH-mode | QD-7 for the signal reading, QD-8 only if a hold is proposed |
| LL-013 command delivery | not reviewed here | intent / enqueue / wire / ack as separate events | Robert (TASK-1412.11 budgets) |
| LL-014 MaxTSet | `test_sat_max_tset.py` | limit change mid-phase, rejected write | QR-1 for observation only |
| LL-015 source profiles | `test_sat_hp_pwm.py`, hourly-limit exemption | full-loop entry per source, entry 113 MM | Q-1527a for heat pumps |
| LL-017 protection toggle | protection-off pinning tests | toggle during each phase | QD-4 |
| LL-018 missing return ignition | `test_sat_no_return_ignition.py` | Data-Invalid/stale, both transports | QD-5, Conf-1371 |
| LL-001..004, 016, 019..023 | outside this task's coverage list | see verification plan | see TASK-1412.8/.9/.10/.14/.15/.16 |

- **Approval.** None for the map. Each blocked gate inherits its question's approver.

## 4. AC2: decision entries of remediation section 2, checked one by one

Verdicts: **agrees** (firmware at HEAD matches the decision, code opened); **agrees, not re-checked at HEAD** (decision recorded and implemented per its task, code not opened in this session); **gap** (decided, not implemented); **open** (entry has an open point); **conflict** (two recorded statements or a statement and the code disagree; queued).

| Entry | Decision (short) | Later entries | Firmware at HEAD | Verdict |
|---|---|---|---|---|
| 1 | Duty (requested - base)/(BT - base); BT EMA 180 s after flame, then held | 64-66, 125, 128 | BT filter and duty mapper not opened | agrees, not re-checked at HEAD |
| 5 | PWM OFF: CS=10 with CH=0 | 9, 17, 60 | `SATcontrol.ino:1376-1379`, `SATchEnable.h:49-51` with protection on | agrees; with protection off see entry 20 row and QD-4 |
| 7 | CH off below 28.2/21, hold floored at cold setpoint | 11, 96, 98 (at or below) | `SATchEnable.h:26-29`, `SATcontrol.ino:974` | agrees (boundary settled by entry 98) |
| 8 | CS during PWM only keeps the flame on; duty lives in pwm.py | none | separate hold/suppression and duty paths (`SATcontrol.ino:966-987`) | agrees |
| 9 | Four-point CH rule; point 1 CH stays 1 in PWM ON (departs from thermo-nova) | 11 (points 3/4), 17 (confirms 1/2) | `SATchEnable.h:46-51` | agrees |
| 10 | No Continuous/PWM buttons; active mode visible | none | `POST /api/v2/sat/mode` kept by our decision; its `pwm` value is a PWM entry path (`SATcontrol.ino:3647-3649`) | agrees; relevant to QD-1 |
| 11 | Solar on the requested setpoint; 28.2/21 everywhere; integral freeze per thermo-nova; answer 1 read by us as CH=1 for a held CS with protection off | 98; ADR-199 records the CH reading | solar `SATcontrol.ino:6653-6655`; cold cutoff `:6750`; CH rule `SATchEnable.h:46-51` | agrees; the protection-off CH reading is ours and goes back to him as QD-4 |
| 12 | Cold cutoff disables PWM and resets the setpoint | 98, 190 | `SATcontrol.ino:6750-6761` | agrees |
| 15 | Port on_cycle_end both branches, margin 2 | 18, 36, 37, 55 | TASK-1331 To Do | gap (tracked, prerequisite TASK-1412.14) |
| 17 | Confirms 5 and 9 on his installation | none | as entries 5/9 | agrees |
| 18 | End-of-cycle switch ignores PWM auto switch | 19 | setting removed (TASK-1341), switch not built | agrees (nothing to gate) |
| 20 | Override on by default for all systems, user can switch off | 7, 11 | `SATtypes.h:505` default true | agrees; with entries 5/17 and our entry 11 reading, whether the OFF phase still switches the boiler off with protection off was never asked directly (R07, QD-4) |
| 21 | MM 0 in PWM for gas; heat pumps/hybrids never 0; Geminox >= 10 | 113 (heat pump always configured max) | `SATcontrol.ino:467-479`: heat pump 100 outside PWM | gap against entry 113 (TASK-1426 To Do); rest agrees |
| 22 | Cold start continuous; flow >= requested + 2.0 for 60 s to PWM | 37, 41, 86 | `SATcycles.ino:1110` uses `>=` | agrees |
| 23 | Hold is a setting (20 s), then flow - 1, refreshed every few seconds | 44 | `SATcontrol.ino:1398-1422`, `:981-987` | agrees |
| 28 | Custom PID, integral only inside deadband, D frozen there | 42, 75-77, 95, 111, 124, 136 | PID code not opened | agrees, not re-checked at HEAD |
| 30 | Device type and heating system separate; heat-pump cycles; ID3 LB2 auto-detect proposed | 40, 50, 52, 82, 186, 188; ADR-157 | `SATcontrol.ino:297-302` manual source | agrees with ADR-157; entry 30's detection proposal was never confirmed and ADR-157 (Accepted) governs. Not queued: no open control question |
| 31 | 180 s minimum; duty ranges from cycles per hour; ultra-low case | 80, 122, 143 | `SATcontrol.ino:446-450` (180 s); mapper not opened | agrees, not re-checked at HEAD (mapper) |
| 36 | Per-sample control setpoint in the tail before the end-of-cycle switch | 55 | TASK-1405 Done; classifier not opened | agrees, not re-checked at HEAD. Open point "shape metrics per sample" was never asked (outside this task) |
| 37 | Margin 2.0, `>=` | 55 | classifier not opened | agrees, not re-checked at HEAD |
| 39 | No valid OT: SAT stops, sends once, then nothing | 47, 79, 121 | `satDisable` not opened | final message replaced by entry 47; not re-checked at HEAD |
| 40 | Heat pump minimum ON 180 s; 1800/2400 s are periods | 50, 181, 182, 184 | `SATcontrol.ino:446-460` | agrees; under revision via Q-1527a |
| 42 | Fixed 60 s PID cadence | 111 | PID service not opened | agrees, not re-checked at HEAD |
| 43 | First PWM phase is ON | 80, 91 | schedule not opened | agrees, not re-checked at HEAD |
| 44 | CS, MM, CH on every control update | none | `SATcontrol.ino:6764-6770` sends every tick (TASK-1370) | agrees (5 s resend path not opened) |
| 45 | No-return ignition: flood CS/PM=25 for 20 s, MI=500 | 84, 135, 141, 142, 149, 151 | `SATcontrol.ino:970-978`, `:1524-1580`, `:1586`, `:2278-2282` | agrees for Unknown-DataID; **open**: entry 84 Open #1 and #3, entry 135 Open (QD-5, Conf-1371). Whether the OT-Direct flood design was shown to him (entry 45 Open #3) not found in the log |
| 47 | Lost OT: hand back CS=0, CH=1, MM=T once, then nothing | 79, 121 | `satDisable` not opened | agrees, not re-checked at HEAD. "What counts as no valid OT" was decided by the maintainer (Q33 A), not by Sergeant D (open-questions.md, 2026-10-29 update) |
| Q33 | What counts as "no valid OpenTherm": A) no boiler reply for 30 s, a Data-Invalid reply counts as alive | answered by the maintainer (Robert, 2026-10-26), asked of Sergeant D four times without an answer (open-questions.md row Q33) | TASK-1365 Done; loss detector not opened | maintainer decision in force, **not a Sergeant D decision**; not re-checked at HEAD. Relevant to R09: whole-link liveness is not per-ID freshness (section 5) |
| Q34 | Resume after OT loss: A) on the first boiler reply | entry 79 (A), refined by 121, 126, 129 | TASK-1365 Done; resume path not opened | agrees, not re-checked at HEAD |
| 48 | User MaxTSet slider, sent to the boiler | 100, 148, 152, 155 | `SATcontrol.ino:314-395` | agrees as refined by 100 and 148 |
| 50 | Heat pump: 180 s ON, 1800 (default) / 2400 s | 181, 182, 184, 188 | `SATcontrol.ino:453-460` | agrees; under revision via Q-1527a |
| 51 | SAT caps every CS at the user MaxTSet | 54, 152 | all CS paths in section 3 R15 | agrees |
| 52 | Defaults 3/h gas, 2/h heat pump | 99 | `SATcontrol.ino:436-442` | agrees |
| 54 | Never a CS above MaxTSet, ignition hold included | none | `SATcontrol.ino:970-978` clamp | agrees |
| 55 | Classifier matches classifier.py exactly | resolution index R40 (input parity open) | TASK-1387 Done; classifier not opened | agrees, not re-checked at HEAD |
| 58 | "Pauses PWM counting" during DHW (statement) | 59, 123, 127 | limiter not opened | superseded by 59/123/127; not re-checked at HEAD |
| 59 | Count only PWM's own ON phases | 81, 89, 90, 105, 127 | limiter not opened | agrees, not re-checked at HEAD |
| 60 | Report: "CH=1 stays during PWM OFF" | none | OFF phase CH=0 with protection on (`SATchEnable.h:50`) | **open** since 2026-10-26 and **no queue row** in open-questions.md (record gap): QD-7 |
| 64 | Alexw: raw flow after start-up (develop) | 65, 66, 125, 128 | firmware follows the pin | open (Robert: follow develop or move the pin; not a Sergeant D item) |
| 65 | Alexw: flow + 10 relight boost (develop) | 66, 125, 128 | no boost | open (Robert) |
| 66 | Alexw: "updated in the newer version" | 125 (develop is older than the pin) | n/a | open (Robert); entry 125 corrects the ancestry reading |

Recent entries that bear on the coverage items, checked the same way:

| Entry | Decision | Firmware at HEAD | Verdict |
|---|---|---|---|
| 84 | No return = Unknown-DataID on 28, "for now" | `SATcontrol.ino:2278-2282` | agrees; Open #1, #3 still open |
| 100 | ID49 bounds, ID57 value, no ID6 gate | `SATcontrol.ino:314-395` | agrees |
| 102 | HB bit 1 does not force PWM | no SAT use of bit 1 found | agrees |
| 113 | Heat pump always configured max MM | `SATcontrol.ino:474` gives 100 outside PWM | gap (TASK-1426) |
| 135 | Valid 'R' sensor counts as return | `OTGW-Core.ino:1145-1159` | agrees; Open (sensor age, '-') |
| 141 | MI=500 on PIC after detection or Ideal/Intergas/Nefit | `SATcontrol.ino:1583-1592` (partly opened) | agrees, not fully re-checked |
| 148, 152, 155 | SH= only when chosen; no global cap above it; heat-pump 40 default | `SATcontrol.ino:330-395` | agrees |
| 179 | Honour HB4 after Sergeant D's OK (maintainer) | not implemented | gap, **blocked** on his OK. Entry 179 says "asked as Q-1519j", but open-questions.md has **no Q-1519j row** (record gap): treat it as not asked; QD-1 to QD-3 below |
| 181, 182, 184 | Heat pumps: fewest starts; may deviate from thermo-nova; study 7200 s | entries 40/50 still implemented | open (Q-1527a asked) |
| 186, 188 | Hybrid takes only the heat-pump cycle | `SATcontrol.ino:453-460` | agrees |
| 190 | Summer simmer cutoff leaves PWM like the cold cutoff (maintainer, without his prior OK) | `SATcontrol.ino:6545-6547` | implemented; conflict noted in the entry itself, Q-1362a posted as a notice. Not re-queued |

No entry conflict was resolved here. No Accepted ADR was edited.

## 5. AC3: measurement states per source

Each state is a separate column on purpose. Where the firmware merges two states, the cell says so.

| Input | Unknown support (Unknown-DataID) | Invalid reply (Data-Invalid) | Missing sample (never / no new one) | Source identity | Sample age |
|---|---|---|---|---|---|
| Flow, ID25 | Gateway bitmap records it (`OTGW-Core.ino:5202`); SAT never consults it for 25 | Not represented: value not stored (`OTGW-Core.ino:1900-1909`, `:2831`), last value kept | Never: 0.0 sentinel (`OTGW-Core.h:80`), read as "no flow" (`SATcontrol.ino:1369`, `:2019`). No new sample: not represented | Not represented: B frames, OT-Direct, and the PS=1 summary (`OTGW-Core.ino:4638`) all write the same field | Not represented. OT-Direct poll period 10 s (`OTDirect.ino:57`, `:146`); PIC path not traced |
| Return, ID28 (OT) | Represented: bitmap plus this-boot confirmation (`OTGW-Core.ino:5198-5203`), used by `satNoReturnTemp()` (`SATcontrol.ino:2278-2282`) | Not represented: last value kept (same store gate) | Never: 0.0 sentinel (`OTGW-Core.h:83`), floored by the hold (`SATcontrol.ino:974`). No new sample: not represented | Not represented between OT frame, PS=1 summary (`OTGW-Core.ino:4641`) and board sensor (`applyFramelessTret`, `:4884`) | Not represented for the OT value |
| Return, PIC board sensor 'R' | n/a (no OT frame) | Represented as "no valid reading" for '-' or garbage (comment `OTGW-Core.ino:1115-1116`) | Represented: `picBoardTretSeen`, poll result count (`OTGW-Core.ino:1118-1120`, `:1145-1159`) | Represented only as "a valid board reading exists"; the value itself lands in the shared `Tret` | Represented: 5 min validity (`OTGW-Core.ino:1117-1124`) |
| Room temperature | n/a (ID24 is written by the thermostat) | Range check -10..50 only (`SATcontrol.ino:2051-2054`) | ID24 never: NAN (`OTGW-Core.h:79`, `SATcontrol.ino:2126`); BLE/external: valid flags (`:2088`, `:2103`) | Priority BLE > zones > external > ID24 (`SATcontrol.ino:2085-2135`), not published as a source field in the code read. Gap: in fallback mode an estimated value is returned as if measured (`SATcontrol.ino:2129-2134`) | Represented per source: BLE 5 min (`:143`, `:2090`), external `iSensorMaxAgeS` (`:2105`), ID24 5 min (`:144`, `:2122-2125`) |
| Outside temperature | ID27 bitmap not consulted; a live Read-Ack stamp is required instead | Not stamped by a non-Ack reply (comment `SATcontrol.ino:2175`, live Read-Ack only) | Represented: no valid source gives false and SAT hands back (`SATcontrol.ino:2183-2216`, entries 56, 57) | Represented: source letter S/E/O/W/R/N (`SATcontrol.ino:2179-2181`) | Represented: 1 h per source (`SATcontrol.ino:2178`, `:2190-2214`) |
| ID3 HB4 (for R01) | Not represented for SAT | Not represented for SAT | Not represented for SAT | Not represented | Not represented. OT-Direct polls every 60 s (`OTDirect.ino:162`) |

Reading of the table: outside temperature is the only input with all five states. Flow and return merge "invalid reply" and "no new sample" into "last value", and merge "never received" with a 0.0 reading. That is the R08/R09 gap. Thermo-nova has only value-or-None for flow and return (section 3, R08 and R09), so separating these states goes beyond parity; the M basis is OT-M4 (three distinct replies), and the fallback per state is SAT policy (QD-5, QD-6).

## 6. Source selection, ownership and cutoff rules (kept explicit)

| Rule | Basis | Code |
|---|---|---|
| Heat source is manual; AUTO = gas; detection is a telemetry hint | ADR-157 (Accepted) | `SATcontrol.ino:290-302` |
| SAT owns MsgID 1 while enabled; one predicate `satOwnsControlSetpoint()` | ADR-179 (Accepted 2026-10-15), `docs/adr/ADR-179-name-the-owner-of-the-opentherm-control-setpoint-while-sat-is-enabled.md:110-113` | firmware predicate not opened |
| Cold cutoff at or below 28.2 radiators / 21 underfloor; PWM off, CS=10, CH=0, MM to max | entries 7, 11, 12, 98; ADR-199 (Accepted 2026-10-29, supersedes ADR-195; the remediation plan's "Proposed" status for ADR-199 is out of date) | `SATchEnable.h:26-29`, `SATcontrol.ino:6750-6761` |
| CH=0 in PWM OFF only when PWM drives the CS; otherwise CH follows the requested setpoint | entries 5, 9, 11, 17; ADR-199 | `SATchEnable.h:46-51` |
| CS, MM and CH go out on every control update, plus a 5 s resend | entry 44; ADR-203 (Accepted 2026-10-30, guideline-level) | `SATcontrol.ino:6764-6770` (resend path not opened) |
| Summer simmer cutoff behaves as the cold cutoff | entry 190 (maintainer; Q-1362a notice to Sergeant D) | `SATcontrol.ino:6545-6547` |
| Valves closed = heater off | entry 32; thermo-nova `heating_control.py:69-70` | `SATcontrol.ino:6565` |
| OT loss: hand back once (CS=0, CH=1, MM=T), then nothing; resume on the first boiler reply | entries 39, 47, 79, 121; Q33 decided by the maintainer | not re-opened (TASK-1365 Done) |
| HB4 veto (when built) overrides PWM entry, never the source or ownership | OT-M1; entry 179 | not implemented |

Nothing in this document changes these rules.

## 7. AC4: control-change subtasks and the approval each needs

Rule for every row: an open question blocks only the named implementation and the validation claim that depends on it. Everything else in the task, and every other task, can proceed. A blocked item stays visible in the task notes and in the release evidence; it is never passed by a host test.

| Task | Changes boiler control? | Required approval | Blocking question(s) | What the question blocks, and only that |
|---|---|---|---|---|
| TASK-1412.4 (HB4) | yes | Sergeant D (entry 179, ADR-196) for the whole veto | his OK, plus QD-1, QD-2, QD-3 | the veto implementation and LL-010; the state representation and the tests of the M meaning can be written now |
| TASK-1412.5 (measurement validity) | yes | Sergeant D | QD-5, QD-6, Conf-1371 | the stale/invalid fallback and its LL-011 claim; recording age and source per sample is not blocked |
| TASK-1412.6 (protection-off semantics) | yes | Sergeant D; Robert for a superseding ADR of ADR-199 if B or C is chosen | QD-4 | any change to OFF-phase actuation with protection off and the LL-017 claim; documenting today's behaviour is not blocked |
| TASK-1412.7 (entry and post-DHW) | only if a change is proposed | Sergeant D for a change | QD-7 (signal reading); QD-8 only after the comparison | QD-7 blocks the CH-enable versus CH-mode acceptance wording of AC#3; the OT-R9 comparison itself is not blocked |
| TASK-1412.8 (exits under jitter) | yes (TASK-1428, TASK-1429) | Sergeant D (task boilerplate) | QD-6 for TASK-1428's flow-validity rule | TASK-1428 only; TASK-1429 and the jitter/rollover traces are not blocked |
| TASK-1412.9 (start counting, phase durations) | only if a change is found | Sergeant D for a change | none open (Q60 answered, entry 143) | nothing |
| TASK-1412.10 (PID verification) | no (verifies decided PID tasks) | none; TASK-1415 needs Robert's write-cadence decision | Robert: TASK-1415 cadence | TASK-1415 and its persistence claim only |
| TASK-1412.11 (command delivery) | yes (transport) | Robert (budgets, ADR-199 design); Sergeant D if CS/MM/CH cadence changes | none queued here | the latency budget claim until Robert sets it |
| TASK-1412.12 (missing-return validation) | no (validation) | none | QD-5, Conf-1371 | the stale/invalid return cases of LL-018 only |
| TASK-1412.13 (MaxTSet verification) | no | none | QR-1 (Robert) | the rejected-write observation only |
| TASK-1412.14 (classifier input parity) | no; gates TASK-1331 | Sergeant D for any kept deviation (resolution index R40) | not queued here | TASK-1331 resumption only |
| TASK-1412.15 (telemetry) | no | Robert (MQTT/HA contract), e.g. the inverted `supports_pump_control` name | none queued here | n/a |
| TASK-1412.16 (model quantification) | no | none to measure; a new controller needs a Proposed ADR and Sergeant D | none | n/a |
| TASK-1412.17 (commissioning profiles) | no (profiles) | Robert and Sergeant D | Q-1527a (asked) | the heat-pump profile and the heat-pump claims in TASK-1412.23 and TASK-1412.25; gas and hybrid profiles are not blocked |
| TASK-1412.18 to .26 (gates) | no | Robert authorises bench, equipment and field steps | inherit every open question above | each gate's claim for the affected behaviour only |
| TASK-1426 (heat-pump MM outside PWM) | yes | none beyond entry 113 | none | n/a |
| TASK-1527 (heat-pump low load) | yes | Sergeant D | Q-1527a | the heat-pump behaviour change only |
| TASK-1331 (end-of-cycle switch) | yes | decided (entries 15, 18, 36, 37, 55) | TASK-1412.14 parity; QD-1 for whether HB4 also gates this path | its resumption only |

Task boilerplate check (grep of the task files, read-only): TASK-1412.4, .5, .6, .7, .8, .9, .11 carry "Before changing boiler control ... obtain and record his required OK". TASK-1412.3, .10, .12 to .26 carry "Preserve recorded decisions and approval requirements" instead.

## 8. Implementation contracts that are unambiguous today

These can be written as tests in TASK-1412.3 now. Items 2 to 6 describe recorded or implemented behaviour. Item 1 is fixed by the specification, but its implementation waits for Sergeant D's OK (entry 179); until then its test is an expected-to-fail contract.

1. ID3 HB4 = 1 from a valid Read-Ack: master low-load not allowed; explicit Unknown-DataID on ID3: SAT uses its own PWM (OT-M1).
2. Unknown-DataID, Data-Invalid and a valid reply are three separate recorded states for ID3, ID25 and ID28 (OT-M4). A 0.0 sentinel is "never received", not a measurement.
3. Every transmitted CS is capped by the effective ceiling on every path (entries 51, 54, 100, 148, 152, 155; section 3 R15).
4. Manual heat source is authoritative; AUTO = gas (ADR-157).
5. PWM OFF with protection on: CS=10 and CH=0 in the same tick (entries 5, 17); CH enable is never inferred from the slave CH-mode bit (OT-M7).
6. Sustained overshoot entry: flow >= requested + 2.0 for 60 s from the first qualifying tick, cleared within 300 s after hot water (entries 22, 41, 86, 108).

## 9. Queued for Sergeant D

Proposed for the main session to add to [open-questions.md](open-questions.md) (this document does not edit it). One question per message, as the queue rules require. IDs are proposals; the main session assigns the final ones. Advice follows the 2026-10-30 ordering (best control result, then his recorded answers, then thermo-nova); it is our recommendation, not a decision.

**QD-1 (R01, entry 179; possibly what entry 179 calls Q-1519j, which has no queue row).** The specification fixes what bit 4 means and makes honouring it mandatory (AFS 2.1.1.2.10, p9); Robert chose to build it after your OK (entry 179). This asks for your OK and one policy choice. The boiler reports MsgID 3 bit 4 = 1 ("master low-load not allowed"). Which PWM entries does that block?
- A) All of them: the 60 s overshoot switch, PWM restored after a restart, the manual `pwm` mode command and the future end-of-cycle switch. SAT stays continuous. Consequence: such a boiler never gets SAT PWM, even if the user asks for it.
- B) Only the automatic ones; a manual `pwm` command still works. Consequence: a user can override the boiler's veto.
- Advice: A. The specification says "The master must switch it off" (AFS 2.1.1.2.10, p9) and gives no manual exception. thermo-nova has no such check (`coordinator/serial/binary_sensor.py:54` only shows the bit).

**QD-2 (R01).** Bit 4 changes to 1 while SAT is already in PWM. What happens?
- A) SAT leaves PWM in that tick and sends the continuous setpoint, as the underheat exit does (entry 134). Consequence: one ON phase may end early.
- B) SAT finishes the current ON or OFF phase, then leaves PWM. Consequence: up to one phase (up to twice the period) of PWM against the boiler's veto.
- Advice: A (the specification has no phase-end allowance).

**QD-3 (R01).** SAT does not know bit 4 yet: no valid MsgID 3 reply since the restart, or the reply was Data-Invalid. SAT asks for MsgID 3 at boot (`PM=3`), OT-Direct reads it every 60 s.
- A) Treat it as allowed, as today. Consequence: PWM may run before the boiler's answer is known.
- B) No PWM until a valid MsgID 3 reply arrives. Consequence: PWM waits for the first reply; if a gateway never gets one, SAT never uses PWM.
- C) B, but at most N minutes, then A with a warning in the log. Consequence: a missing reply is treated as "allowed" after N minutes, which the specification does not say.
- Advice: B, because the specification makes reading MsgID 3 at start-up mandatory and does not treat silence as "allowed". Risk: a PIC gateway where nobody reads MsgID 3 (not traced) would never enter PWM; C is the fallback if that turns out to happen.

**QD-4 (R07; confirm or revise our reading of entry 11 answer 1).** On 2026-10-23 we asked what CH should be with overshoot protection off in PWM; we read your answer 1 as CH=1 (recorded in ADR-199). Whether the OFF phase should still switch the boiler off was never asked directly. Overshoot protection is switched off while SAT runs PWM. Today the OFF phase keeps the previous setpoint with CH=1 (and MM=0 on a gas boiler), as in thermo-nova (`heating_control.py:385`, `:238`, `:139-140`), so SAT never switches the boiler off.
- A) Keep this (thermo-nova parity). Consequence: switching protection off also removes the OFF phase.
- B) The OFF phase always sends CS=10 with CH=0 (entries 5, 17); protection off only skips the ignition hold and the suppression of the ON phase. Consequence: a deviation from thermo-nova and a reversal of our entry 11 reading (ADR-199 would need a successor); on/off cycling stays with protection off.
- C) Protection off means no PWM: SAT stays continuous. Consequence: boilers that honour MM lose PWM; ADR-199 would need a successor too.
- Advice: B. It matches the specification's OFF period (AFS 2.1.1.2.3, p6: clear CH enable) and his own entries 5 and 17.

**QD-5 (R08; re-ask of entry 84 Open #3 and entry 135 Open, not a new topic).** The boiler answers MsgID 28 with Data-Invalid, or the return temperature stops arriving. Today SAT keeps using the last value.
- A) After N minutes without a valid value, treat the boiler as "no return temperature" (flow + 18 start, flood, entries 45, 149, 151). Consequence: a boiler with a broken sensor gets the no-return start-up.
- B) Keep the last value (today). Consequence: the hold can be built on an old return temperature.
- C) Hold at the cold setpoint until a valid value returns. Consequence: weaker ignition setpoint while the sensor is out.
- Advice: A, with N the same 5 minutes the board sensor uses (our choice, `OTGW-Core.ino:1117`). The same question answers the board sensor's '-' reading (entry 135 Open).

**QD-6 (R09).** The flow temperature (MsgID 25) gets a Data-Invalid reply or no new value for N seconds. Today SAT keeps using the last value, also for the 3-second suppression.
- A) Treat it as missing: PWM goes idle and sends the continuous setpoint (what thermo-nova does for a missing flow, `pwm.py:122-128`, and what the firmware does today only for 0.0), and the 60 s / 180 s / 300 s timers reset (TASK-1428). Consequence: a short bus gap gives idle ticks.
- B) Keep the last value (today). Consequence: the suppression can follow a frozen flow temperature for as long as the link stays up.
- Advice: A, with N = 30 s (three OT-Direct polls; our proposal, to be measured on the bench).

**QD-7 (record gap: entry 60 Open since 2026-10-26, no queue row).** Re-ask, with the frames of entry 60: "CH=1 stays during PWM OFF": did you mean SAT's CH enable (MsgID 0 high byte bit 0) or the boiler's CH-mode bit (MsgID 0 low byte bit 1, likely pump overrun)?
- A) The boiler's CH-mode bit: no change, the graph label needs to say so. B) SAT's CH enable: please name the frame. Advice: A, per the capture checked in entry 60.

**QD-8 (R25; ask only after TASK-1412.7 has the comparison traces).** After a hot-water draw the specification recommends keeping the boiler on by setting the control setpoint to the boiler temperature for one minute (AFS 2.2.3, p17). SAT today relies on its continuous clamp (CS >= flow - 2 with the flame on) and the PWM resume rules.
- A) Keep SAT's behaviour. B) Add the one-minute hold in continuous mode. Advice: A unless the traces show unneeded boiler stops after draws.

**Conf-1371 (confirmation of our implementation choice, entry 84 Open #1).** For "no return temperature" the firmware requires the Unknown-DataID answer to come from this boot, and counts a board sensor reading as valid for 5 minutes (`SATcontrol.ino:2268-2277`, `OTGW-Core.ino:1117-1124`). A) OK. B) Use the stored verdict at once after a restart. Advice: A (a stored verdict alone would count a board with an 'R' sensor as "no return" for its first minutes).

Existing questions referenced, not re-queued: **Q-1527a** (heat pumps, asked 2026-10-31), **Q-1362a** (summer simmer notice, entry 190).

### Queued for Robert (not SAT design)

**QR-1 (R15).** A boiler rejects `SH=` (Unknown-DataID or Data-Invalid on the MsgID 57 write). SAT keeps capping its own CS either way (entries 51, 54). A) Keep today: send once per change or restart, no observation. B) Read the gateway's write verdict (`isBoilerMsgIdUnsupportedWrite(57)`, `OTGW-Core.ino:5205`) and publish it. C) B, and stop sending `SH=` to that boiler. Advice: B. The protocol has no rule that forbids the write (OT-R11), so this is observability, not compliance.

## 10. Evidence from this session

- Revision: `git rev-parse HEAD` = `c2ed654ec01f7dee134ff150c5c8b0a742c7d0f6`; SAT and OTGW-Core sources clean at that commit.
- Host tests: see the run record below. Host tests compile extracted firmware functions with a host compiler; they are not a firmware build and not a bench or boiler result.

  - Command, from `tests/`: `python -m unittest test_sat_pwm_override test_sat_no_return_ignition test_sat_max_tset test_sat_overshoot_sustain test_sat_hp_pwm test_sat_pwm_anchors test_sat_pwm_start_limit`.
  - Result: `Ran 257 tests in 66.772s`, `OK`, exit code 0. HEAD when the run started: `cd5071ebb3f38c4b038529754cda6ab20e7eabe2`.
  - HEAD moved during the session (c2ed654ec, then cd5071ebb, then 3fc03a369, other sessions' commits). `git diff c2ed654ec 3fc03a369` over `SAT*`, `OTGW-Core.ino`, `OTGW-Core.h`, `OTDirect.ino` and the seven test files changes only the version banner line (alpha.588 to alpha.591) in seven SAT files, so every line number above holds at all three commits.
  - These tests pin today's behaviour. Where this document calls a behaviour "blocked", a green test is not an approval of it. Red-then-green was not re-run for any of them.

- Not verified in this session: PIC-gateway reading of ID3 and ID25 (who polls, how often); OT-Direct `boilerEvidence` for MsgID 28 answers (entry 84 Open #1); the BT filter, duty mapper, hourly limiter, PID, classifier, `satDisable` and the 5 s resend (rows marked "not re-checked at HEAD"); AFS figures (PDF not rendered); every thermo-nova location marked "not re-opened". No physical behaviour is claimed anywhere.

## 11. thermo-nova ranges opened in this session

`heating_control.py:60-75, 130-160, 197-240, 262-320, 380-400, 450-492`; `pwm.py:104-135`; `device/types.py:1-26`; `coordinator/__init__.py:178-195`; `coordinator/mqtt/opentherm.py:100-120`; `coordinator/serial/binary_sensor.py:50-56`; `const.py:19, 21, 40` (by grep); `cycles/const.py:10` (by grep); `climate.py:540-548`. Any other thermo-nova citation in this document is quoted from a decision entry and marked so.
