---
id: TASK-1537
title: >-
  SAT Immergas quirk sends TP=11:12=<setpoint> instead of thermo-nova's min(max
  modulation, 80)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T02:01:00Z'
updated_date: '2026-11-02T20:40:00Z'
labels:
  - sat
  - bug
dependencies: []
priority: high
ordinal: 693000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.17 profile work, second-read 2026-11-01. Firmware SATcontrol.ino:6772-6777 sends TP=11:12=<finalSetpoint> with every control update for Immergas. thermo-nova sends TP=11:12={min(value, 80)} together with MM=, where value is the max relative modulation (coordinator/mqtt/opentherm.py:214-219 at 7beaaf5). Entry 21 is the related Sergeant D record (check its wording before the fix). Sending a flow setpoint into the Immergas TSP 11/12 (max modulation) parameter may set a wrong modulation limit on Immergas boilers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Entry 21 and the Immergas quirk origin checked; conflict (if any) put to Sergeant D before the change
- [x] #2 Host test on the real code: with the Immergas quirk TP=11:12 carries min(max relative modulation, 80), sent with MM= (red on HEAD, green)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: SATcontrol.ino:6809-6812 still sends TP=11:12=<finalSetpoint>. Check entry 21 first. Agent-doable now.

2026-11-02 AC1 check: entry 21 (sergeant-d-corrections.md:869, :892) only records thermo-nova's TP=11:12={min(value,80)} before MM and that the port was not checked; entry 44 area (:2421) lists it as not settled. thermo-nova opened at the pin: coordinator/mqtt/opentherm.py:216-219 (TP before MM, value = max relative modulation, MM uncapped). Firmware: SATcontrol.ino ~6840-6846 sends TP=11:12=<finalSetpoint> after CS/MM/CH each tick; satComputeMM caps MM itself at 80 for Immergas (:506), no Sergeant D record for that cap. Two differences -> Q-1537a queued in docs/sat/open-questions.md (A follow thermo-nova, our advice; B keep MM cap too; C no TP). Not posted: Sergeant D away since 06:08 UTC and the Q-1539a keepalive sub-question is re-asked first when he is back. Task waits for his answer (SAT behaviour change, implement-next-task step 4).

2026-11-02 entry 233: Sergeant D answered Q-1537a A (msg 1566847427238035578): TP=11:12=min(MM,80) right before every MM=, MM uncapped. Implementation heads-up from the TASK-1315 session: the PIC queue spaces PM=/TP= (writes too) by up to 1 s (PRIO_MSG_SPACING_MS, OTGW-Core.ino handleCommandQueue) and MM= is not held, so MM= can overtake the TP=; TP= is unique per value, so a changing MM leaves several TP=11:12=x entries. Options: enqueue MM= only after the TP= went out, or prove the order with tests/test_pic_pm_chain.py's send-time harness.

2026-11-02 evidence: tests/test_sat_immergas_tp.py 4/4 (HEAD copy: 3 fail). test_sat_pwm_override immergas case 100 (was 80). Five harnesses (cmd_cache, ot_loss, push_setpoint, restart_quiet, no_return_ignition) got a satImmergasTpActive stub, as for TASK-1536. Full tests/ 95/95 (run in parts: a background run was stopped for low PC memory, not a test failure). evaluate --quick 0 failed. Line-by-line thermo-nova check: value, format, order, call sites, MM uncap, PWM TP=0, ~5 s cadence match; TP only in thermo-nova's MQTT OTGW coordinator, not its serial one; MemberID 27 = Immergas and Sime -> beta.16 (entry 235).

beta.16: TP only with explicit Immergas (entry 235, Robert). test_sat_immergas_tp 5/5; beta.15 copy fails the Auto/MemberID-27 case.

2026-11-02 bench beta.17 (.63, 3e928d9): satmanufacturer Auto (0) 75 s telnet: 0 TP=11:12, 10 MM=100; Immergas (8) 60 s: 12x cmd[TP=11:12=80] queued 1 ms before cmd[MM=100], every 5 s; restored to 0. FINDING: the command queue updates an existing slot in place (MM in slot 1, TP in slot 3, dispatch order 0..3), so at the PIC MM= goes first and TP= after it (PM/TP spacing up to 1 s). Entry 233's implementation note asked for this check; my host test only checks the enqueue order. thermo-nova via OTGW-firmware has the same queue behaviour. Put to Sergeant D as Q-1537b (msg 1566908020888310011), advice A keep.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Immergas quirk aligned with thermo-nova (coordinator/mqtt/opentherm.py:216-220, entry 233) and limited to an explicit choice (entry 235). beta.15 9e123585b: satSendMM() queues TP=11:12=min(mm,80) right before every MM= (tick, 5 s resend, hot water, cutoff), MM no longer capped at 80, the tick-level TP=11:12=<flow setpoint> removed. beta.16 5a64db5d7: satImmergasTpActive() = settings.sat.iManufacturer == Immergas, so Auto (MemberID 27 = Immergas and Sime) sends no TP. Evidence: AC1 entries 21/233/235 checked, Q-1537a answered A. AC2 tests/test_sat_immergas_tp.py 5/5 (old code 3/4 fail; beta.15 fails the Auto/27 case), full tests/ 95/95, evaluate --quick 0 failed, build esp32-combo beta.17 (2x 'Successfully created ESP32S3 image', 'Build completed successfully'), bench .63 telnet: Auto 0 TP in 75 s, Immergas 12x cmd[TP=11:12=80] each 1 ms before cmd[MM=100] in 60 s. Finding: the queue updates slots in place, so at the PIC MM= goes before TP= (~1 s), same as thermo-nova on an OTGW; Q-1537b to Sergeant D (msg 1566908020888310011, advice A keep); if he picks B that becomes its own task. Not verified: a real Immergas boiler (none on the bench). Pushed 3e928d919, zip msg 1566914637587415100.
<!-- SECTION:FINAL_SUMMARY:END -->
