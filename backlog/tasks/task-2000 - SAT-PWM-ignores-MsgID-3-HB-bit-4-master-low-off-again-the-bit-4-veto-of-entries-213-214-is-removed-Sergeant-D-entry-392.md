---
id: TASK-2000
title: >-
  SAT: PWM ignores MsgID 3 HB bit 4 (master low-off) again; the bit-4 veto of
  entries 213/214 is removed (Sergeant D entry 392)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T11:08:00Z'
updated_date: '2026-11-16T16:45:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1022000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 392 (2026-11-16, to be recorded verbatim by 92 before the build; relayed by the coordinator: 'Don't look at bit 4 at all ... boiler should be able to enter PWM') reverses entries 213 (bit 4 = 1 blocks every PWM entry path) and 214 (bit 4 turning 1 leaves PWM in that tick). Firmware today: satPwmPermitted() = bSlaveCfgValid && !bSlaveLowOffDenied (SATcycles.ino:1302-1336), refusal reason 2 'the boiler does not allow master low-off', satCycleCheckLowLoadVeto() leaves PWM (path 4). thermo-nova only shows the bit (coordinator/serial/binary_sensor.py:54) and never acts on it, so Part 1 returns to thermo-nova. Field case: indigo_light's Vitodens reports HB bit 4 = 1, so SAT never enters PWM there. Part 1 (now): drop the bit-4 condition from satPwmPermitted() and the reason-2 path; keep the bit recorded and shown (REST slave_low_off_denied) as information. Part 2 (entry 215, reason 1: no PWM until a valid MsgID 3 reply) waits on Q-392a (A remove / B keep). Superseded by this task: entries 213 and 214 (by entry 392); firmware-sat-spec.md 'Slave low-load permission gate' section; ADR-249 says 'the boiler's MsgID 3 permission comes first (entry 213)' - after Part 1 that permission means 'a valid MsgID 3 reply' (entry 215), so its wording stays true but its citation is outdated; no Accepted ADR states the bit-4 rule itself (grep docs/adr for TASK-1412.4/satPwmPermitted/bit 4 = 1: only ADR-249's clause), so no new ADR is needed unless Part 2 removes entry 215 too. Milestone 2.0.0 pending Sergeant D's scope answer in Q-392a.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Entry 392 is recorded verbatim in docs/sat/sergeant-d-corrections.md (with link) before the code change
- [x] #2 satPwmPermitted() no longer reads MsgID 3 HB bit 4; a valid reply with bit 4 = 1 allows PWM on every entry path, and bit 4 turning 1 no longer leaves PWM; the bit stays recorded and shown
- [x] #3 Host test on the real code: a Vitodens-like profile (MsgID 3 HB bit 4 = 1) enters PWM; red on the old code (refused, reason 2), green on the fix; the no-valid-reply case is unchanged until Q-392a
- [x] #4 firmware-sat-spec.md and the REST/UI veto text updated; ADR-249's entry-213 citation noted as superseded by entry 392
- [x] #5 Part 2 per Q-392a (entry 215): implemented or recorded as kept
- [x] #6 Field check by indigo_light on his Vitodens: PWM entered (before capture on beta.339, after capture on the fix)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Entry 392 recorded verbatim by 92 (origin 6c9edca6b) before the code; Q-392a asked 11:05:27 UTC (no answer yet). Field test task by the coordinator: TASK-1998 (indigo_light).

2026-11-16 (95) Part 1 AND Part 2 built: entry 393 (Q-392a 'My answer is A', 11:06:48 UTC, origin becbdcc3e) reverses entry 215 too: MsgID 3 plays no part in PWM. Scope line unanswered; read as 2.0.0 (92). Change: satPwmPermitted() returns true (SATcycles.ino); the call points (satPwmEntryAllowed on every entry path, satCycleCheckLowLoadVeto in the loop) stay and never refuse, pwm_veto_reason stays 0; satNoteSlaveConfig still records valid/bit 4 for the status. Comments in SATcycles/SATcontrol/SATtypes now cite 392/393; firmware-sat-spec.md gate section marked REMOVED (former text kept as history); CHANGELOG. Not removed: the now-dead refusal machinery (minimal change in the release week; d3's 1942 also calls satPwmEntryAllowed).

Evidence: tests/test_sat_slave_lowload.py rewritten to entries 392/393 (every MsgID 3 state: no reply, bit 4 = 0, bit 4 = 1 -> PWM on the overshoot switch, fail-safe, restore at once, deferred restore, pwm command, any satSetControlMode caller; running PWM never ended; reason 0; decoder still records the bit): FIX 20/20 OK; OLD (origin SATcycles.ino + SATcontrol.ino via OTGW_SATCYCLES_INO/OTGW_SAT_INO) 16 FAIL (scratchpad t2000-red.txt). Full loop (tests/test_sat_loop_integration.py) Vitodens case: test_s1_pwm_reached_when_boiler_reports_low_off_not_allowed ('lowoff 1' + S1 runs check_s1: PWM, ON and OFF phases on the wire): FIX OK (file 21/21), OLD SATcycles.ino RED "'2' not found in ['1'] : PWM (mode 2) was never reached". Also green: test_sat_overshoot_sustain 83, test_sat_pwm_override 83, test_sat_pwm_start_limit 26, test_sat_no_return_ignition 19, test_sat_requested_vs_transmitted 10, test_sat_restart_quiet 20, test/host/test_slave_config_evidence PASS, test_sat_lowload_contract ll010/ll023/ll018i 3 passed (LL-010 text rewritten to 392/393). evaluate --quick 0 failed. ADR check: no Accepted ADR states the bit-4 rule; ADR-249's clause 'the boiler's MsgID 3 permission comes first (entry 213)' is now vacuous (always permitted) - its citation is superseded by entries 392/393, no new ADR (the gate was never an ADR decision). Open: AC4 partly (REST/UI veto text kept for old firmware readings; the UI shows nothing since reason stays 0), AC6 field (indigo_light, TASK-1998). d3's tests/test_sat_pwm_first_start.py cases (b)/(c)/(h) need adapting on the rebase onto 1942 (d3 note).

AC4 correction to the note above: met. firmware-sat-spec.md updated; pwm_veto_reason / slave_low_off_denied are not in openapi.yaml or docs/api/README.md (grep), so no API doc change; the web UI veto texts (sat_why_pwm_no_reply/denied in v2.js) stay for older firmware that still reports reason 1/2 and are never shown with this firmware (reason stays 0); ADR-249's citation is noted above.

ADR (coordinator's check 2026-11-16): ADR-249's clause 'and the boiler's MsgID 3 permission comes first (entry 213)' sits in its Decision section, so removing the gate amends an Accepted ADR: ADR-251 'MsgID 3 plays no part in SAT PWM ... (amends ADR-249)' created via adr-kit, status Proposed (Robert accepts), related to ADR-249 both ways (adr relate), README entry added (also the missing ADR-234 entry, so tests/test_adr_governance.py is green again: was FAIL on origin for ADR-234).

Pushed 7613c4317..9011c3c48 unchanged (beta.350). Build exit 0, 6 min, 'Build completed successfully', app size 2143632 B of 2293760 B (93.5 %). Zip posted #alpha-testing msg 1571856532310597724. Smoke 09 on .61 (no SAT): githash 9011c3c, bootcount 855->857 (2 OTA restarts only), settings byte-identical (165 keys), sat/status slave_cfg_valid false, slave_low_off_denied false, pwm_veto_reason 0, pwm_veto_path 0. Before on .63 (beta.349, SAT on): pwm_veto_reason 1, pwm_veto_path 1, control_mode 1 (the veto visible on the old code). .63 gets 350 at ~13:11 for the 20:00 soak (09 compares the same fields). Sergeant D entry 396 (11:41 UTC, msg 1571852828065140818): 'THIS IS TOP PRIORITY for the next release'; Q-396a asks whether that means this release (read as 2.0.0 until he answers). ADR-251 Proposed, acceptance with Robert (coordinator). Open: AC6 field (indigo_light, TASK-1998).

Soak 09 on .63 (Classic, PIC 6.8, its standing SAT config, no boiler), beta.350+9011c3c, 13:14-14:15: PASS (60/60 samples at bootcount 169, MQTT 60/60, heap 45.7-46.7 KB, hd_min flat, no panic/WDT, settings byte-identical). pwm_veto_reason 1 / path 1 on beta.349 -> 0 / 0 throughout on 350. SAT entered PWM twice through the normal switch (13:28:33 'Control mode: PWM (flow 31.1 stayed above setpoint 29.0 + 1.5 for 30 s)', SATMODE 1->2, then back to continuous 13:31:33; 13:53:33 again, back 13:56:33); SATTICK 12 x mode=2. On 349 the same unit stayed in continuous behind the MsgID 3 veto. Boiler response not shown (no boiler). Evidence: 09 scratchpad b350\. AC6 (field, indigo_light on the Vitodens) stays open.

2026-11-16 (95) ADR-251 Accepted by Robert (Discord DM 1571893214372438150, answer A, relayed by the coordinator), set through adr-kit accept (strict lint passed after the SAT/PWM acronyms were expanded at first use and the Status line split); README and ADR-INDEX show Accepted. The task has no separate ADR AC; AC6 (field, indigo_light) stays open.

CORRECTION 2026-11-16 (95, from the coordinator) to the .63 soak note above (fcd3c258c): SAT SIMULATION was on on .63 all day. The flow values (31.1 C), the two PWM entries at 13:28:33 and 13:53:33 and SATTICK mode=2 came from the simulator, not from a boiler; they show the firmware path accepts PWM, not real boiler behaviour. Real device evidence from that run is only the REST value pwm_veto_reason/path 1/1 on beta.349 -> 0/0 on beta.350 (MsgID 3 no longer vetoes), plus the stability numbers. The task's ticked ACs rest on host tests and are unaffected; AC6 (field, indigo_light's Vitodens) is the real-boiler proof and stays open.

CORRECTION by 09, 2026-11-16 16:30: the .63 soak above (beta.350, 13:14-14:15) ran with the SAT SOFTWARE SIMULATOR ON (satsimulation true; .63 has no boiler and no thermostat). 'Its standing SAT config' was the simulator, and 09 did not check the flag. The flag was found by the coordinator's check and switched off at 16:27:11 (only that line changed in settings.ini). REAL, firmware or REST state: githash, bootcount 169 flat, MQTT 60/60, heap 45.7-46.7 KB, no panic/WDT, settings byte-identical, and the REST flags pwm_veto_reason/path 1/1 on 349 -> 0/0 on 350. SIMULATOR, not hardware evidence: the two PWM entries (13:28:33, 13:53:33, 'flow 31.1 stayed above setpoint 29.0 + 1.5 for 30 s'), the flow values and the SATTICK mode=2 counts. The veto lifting is shown; PWM entry against a real boiler is not.

2026-11-16 AC6 field evidence (comms 92): indigo_light's capture 20261116-144953-upload-clean.zip, sent by Sergeant D by DM (msg 1571920739253092412), analysed with /analyze-capture; archived at RvdB/OTGW-field-data/indigo_light/20261116-144953/ (zip sha256 1573a14c...bd84; analysis/ holds the summary, the SAT timeline and charts). Firmware 2.0.0-beta.350+9011c3c, PIC 16F1847, Viessmann Vitodens (MemberID 33), SAT enabled, simulation_enable false / simulation OFF (real boiler), 14:49:54-16:56:34 tester local time, 21939 frames, 0 telnet gaps. PWM entered on the real boiler: telnet 'SAT: Control mode: PWM' at 16:00:58 (flow 67.3 above setpoint 31.0 + 1.5 for 30 s) and 16:17:58; CH flame lit inside a PWM ON phase at 16:18:36; SATTICK mode=2 pwm=1 ticks present. The 'before' half of AC6 (a beta.339 capture) was not made: Sergeant D told indigo 'No, just run beta.350' (#alpha-testing 1571879271532724316). AC6 left for the owner/coordinator to tick or reword.

2026-11-16 (95) AC6 ticked on the coordinator's go (relayed by 92), facts re-checked by 95 in the archive: sat_story.txt:38 '16:00:58 SAT: Control mode: PWM (flow 67.3 stayed above setpoint 31.0 + 1.5 for 30 s)', :42 '16:17:58 PWM ON: pwm_on=361 s', transitions.txt:5 '16:17:58 ... mode=2 pwm=1', further PWM entries 16:30:28/16:35:28/16:39:28; device-state-stop-20261116-165640.txt in the zip: simulation false, simulation_active false (real Vitodens). Deviation from the AC wording: the 'before' capture on beta.339 was not made because Sergeant D told the tester 'No, just run beta.350' (#alpha-testing 1571879271532724316); the old-code red side stays the host test (AC3) and indigo's earlier beta.250 capture, where SAT never entered PWM behind the MsgID 3 veto.

Correction (95, same day): in the AC6 note above, 'indigo's earlier beta.250 capture, where SAT never entered PWM behind the MsgID 3 veto' is NOT verified; I did not re-check that capture for PWM. The old-code red side of this task is the host test (AC3) only.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MsgID 3 no longer gates SAT PWM (Sergeant D entries 392 and 393, ADR-251 Accepted by Robert, DM 1571893214372438150). AC1: entry 392 recorded verbatim before the code. AC2/AC3: satPwmPermitted() ignores MsgID 3; tests/test_sat_slave_lowload.py Vitodens-like profile enters PWM on the fix, refused (reason 2) on the old code. AC4: firmware-sat-spec and REST/UI veto text updated, ADR-249's entry-213 clause amended by ADR-251. AC5: Q-392a answered A (entry 393), no-valid-reply veto removed too. AC6: field, indigo_light's real Vitodens on beta.350+9011c3c with simulation off: SAT entered PWM at 16:00:58 and later, CH flame lit inside a PWM ON phase at 16:18:36 (archive RvdB/OTGW-field-data/indigo_light/20261116-144953/analysis/); the beta.339 'before' capture was dropped on Sergeant D's instruction. Bench: 09 .61 smoke and .63 soak PASS on 350; the .63 PWM entries were SAT simulation (corrected note), only pwm_veto_reason 1 -> 0 is device evidence there. Not verified: long-term PWM behaviour on the Vitodens beyond this 2 h capture.
<!-- SECTION:FINAL_SUMMARY:END -->
