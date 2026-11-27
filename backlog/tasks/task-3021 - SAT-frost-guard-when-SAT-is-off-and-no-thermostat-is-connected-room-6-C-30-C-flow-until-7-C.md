---
id: TASK-3021
title: >-
  SAT: frost guard when SAT is off and no thermostat is connected (room <= 6 C
  -> 30 C flow until 7 C)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T18:47:00Z'
updated_date: '2026-11-27T05:49:00Z'
labels:
  - sat
  - safety
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1135000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Decision: entry 460 in the private SAT register (Robert, 2026-11-21, Q-456a = B). When SAT is switched off and no thermostat is detected behind the gateway (OT-Direct master mode without a thermostat), today the gateway sends CH enable off with no CS override (OTDirect.ino:1259-1271), so nothing protects the house from freezing. With a thermostat, SAT off hands back (satSendRelease SATcontrol.ino:882-888) and the thermostat's frost protection applies: unchanged. Reference: Alex ba515c8 demand_control.py:610-622, :815-828, const.py:31-32. Room temperature from the room source of entries 457/458; no room temperature known -> no action. Open per entry 460: fixed 6 C or a setting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 With SAT off, no thermostat detected and room <= 6.0 C, the gateway sends CH enable on with a 30 C flow setpoint; at room >= 7.0 C it returns to CH off
- [x] #2 With a thermostat connected, behaviour is unchanged (hand-back only)
- [x] #3 No room temperature known -> no frost demand
- [x] #4 Host test on the real code: red on the old code (no demand at 5 C), green on the fix; bench or simulation check on OT-Direct master mode
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (otgw-dev, 2026-11-21; code after TASK-3011 lands). Entry 460 (Q-456a = B). Reference only (ADR-292): ba515c8 demand_control.py:610-622 (_update_frost_protection: on at room <= threshold, off at >= threshold + 1.0, no room temperature -> off), :815-828 (HVAC off + frost -> demand with setpoint max(minimum_setpoint, 30)), const.py:31-32.
1. Where: OT-Direct only (src/OTGW-firmware/OTDirect.ino), the gateway's own status/TSet. Condition 'no thermostat detected' = !state.otd.bThermostatConnected (set/cleared by the thermostat timeout, OTDirect.ino:3565-3600), in master mode, and in gateway mode once the thermostat is gone (the same case otdOwnFramesAllowed() lets the gateway send its own frames). Plus SAT not owning the setpoint (!satOwnsControlSetpoint()) and no external CS= override in force (!otdCsOverrideActive(): a user CS= keeps priority). PIC boards (Classic) are out of scope: the PIC itself sends the standalone frames.
2. Rule: a pure helper otdFrostGuardStep(bool active, float room) between // --- otdFrostGuard begin/end markers (testable like otdOwnFramesAllowed): room NaN -> false; room <= 6.0 -> true; room >= 7.0 -> false; else keep. Constants OTD_FROST_ON_C 6.0 and OTD_FROST_OFF_C 7.0 (fixed for now; Q-460a 'fixed or setting' is queued to Sergeant D, a setting is a small follow-up). Evaluated once per status build on the loop task.
3. Room temperature: the SAT room resolver (satGetRoomTemp(), after TASK-3011 the primary-source order; without a thermostat MsgID 24 is NaN, so external MQTT/REST, pinned BLE or areas), through a non-static getter in SATcontrol.ino. None known -> no action (AC3).
4. Effect while active: buildStatusRequest() keeps CH enable (bit 0) on (today it clears it at OTDirect.ino:1265-1266 when there is no CS override and SAT does not own the setpoint), and the gateway's MsgID 1 WRITE carries 30.0 C (the TSet path the CS override uses; locate in the master scheduler, frost value only when the override is not active). On exit: back to CH off and no TSet, as today.
5. Visibility: one telnet/debug line on enter and exit ('OTD: frost guard on, room X <= 6.0, CH on with 30 C' / 'off, room X >= 7.0'); no new MQTT/REST contract in this task (YAGNI; say if Robert wants one).
6. Tests (AC4): host test that compiles the real otdFrostGuardStep() and the real buildStatusRequest() bit-0 rule: room 5.0 no thermostat -> bit 0 on + TSet 30 (red on today's code: bit 0 off), 6.5 stays on, 7.0 off, NaN -> off, thermostat connected -> unchanged, CS override active -> override wins, SAT owning CS -> unchanged. Bench/sim by 09 on OT-Direct master mode with the room fed over MQTT (sat indoor temp) at 5.0 / 7.5.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (coordinator review of the plan, otgw-dev): approved with two changes. (a) Known limit, to be stated in the ADR text too: PIC (Classic) boards are out of scope. Finding (code reading only, not bench-verified): the PIC's own standalone MsgID 0 sets CH enable only while a CS= is in force (otgw-6.6 gateway.asm readstatus :2712-2745, the SysCtrlSetpoint / HeatDemand mask on bit 0; the firmware reads it the same way in OTDirect.ino:1259-1264, TASK-1806). So a Classic with SAT off and no thermostat also has CH off and no frost protection; this task does not change that. (b) Visibility: a safety action that heats must be visible, so besides the debug line one status field frost_guard_active (bool) in /api/v2/sat/status; no new MQTT topic or HA discovery.

2026-11-21 (Robert, DM 1573773591164485663, option B): the frost guard also covers the Classic/PIC in 2.0.0. Plan update (otgw-dev): the shared rule stays otgw-agnostic (otdFrostGuardStep -> rename to a board-neutral satFrostGuardStep in SATcontrol.ino, same 6.0/7.0 hysteresis, no room temperature -> off, SAT not owning CS, a user CS= keeps priority, status field frost_guard_active). Two outputs: (1) OT-Direct: as planned (bit 0 kept + own MsgID 1 = 30.0). (2) PIC: on entry the ESP sends CS=30 and CH=1 through addCommandToQueue() (the only PIC command path, firmware rule); on release at 7.0 it sends CS=0 and CH back to the value before entry (the PIC then sends its own standalone status again: CH enable off without a CS, gateway.asm readstatus :2712-2745); the guard re-sends CS=30 periodically like SAT's CS repeat (ADR-235 30 s) so a PIC reset does not drop it. New piece of work found: the firmware has no 'thermostat seen' state for PIC boards (the PIC lines 'Thermostat connected/disconnected' are only recognised as markers, OTGW-Core.ino:5757-5786). Plan: derive it from the thermostat's own frames (a 'T' frame within the last 60 s, the PIC's own disconnect timeout order of magnitude) and/or those PIC lines; host-tested. A 'user CS=' on PIC: the guard only acts while no CS= override of another source is known (SAT off; the command path marks a user CS=; to be located in the command handler, otherwise documented as a limit). Bench: OT-Direct master (OTGW32) and a Classic+S3 unit; .63 is in its soak until Sunday ~17:00, so the Classic bench after that or on another Classic unit of 09's.

2026-11-21 (coordinator, two checks for the code): (1) 'thermostat seen' from T frames: verify on a real Classic capture without a thermostat that the PIC's own standalone requests are not logged as T. Expectation (unverified, from the OTGW log format: T = thermostat, R = request made or changed by the gateway): they appear as R; if a capture shows T, the PIC's 'Thermostat disconnected' line becomes the primary signal. (2) The periodic CS=30 repeat stops at once when the guard releases or a user CS= arrives (host test case). If a user CS= is not cleanly detectable on the PIC path, it is documented as a known limit.

2026-11-21: scope extended by Robert (entry 461, private register): the frost guard also covers the Classic with the PIC (no thermostat seen, SAT off): CS=30 + CH=1 through the PIC queue while room <= 6 C, release at 7 C. Basis: PIC 6.6 sets CH enable only while a CS= is in force (otgw-dev code reading of gateway.asm :2712-2745, not bench-verified). Needs a PIC bench test on a Classic unit. Threshold question Q-460a still open.

2026-11-21 implementation (otgw-dev, lane/d3-frost), changed after an advisor review of the plan: ONE output for both boards: CS=30.0 + CH=1 through addCommandToQueue() (satOwn), CS=30.0 repeated every 30 s, CS=0 on release. OT-Direct needs no buildStatusRequest() edit: CS= sets the MsgID 1 override, so otdEffectiveMasterFlags keeps bit 0 on (OTDirect.ino handleOTDirectCommand CS branch). 'No thermostat' = state.otBus.bThermostatState false (no T frame for 30 s, OTGW-Core.ino evaluateOTBusLiveness, both boards funnel through processOT; gateway.asm 6.6 :2490-2495 prints the PIC's own requests as R/A, so standalone frames never count as a thermostat; code reading, a Classic capture still to confirm). Room: measured sources only (satFrostRoomTemp: primary/BLE/areas/external/MsgID 24; no simulation, estimate or SSI; _sat_roomSrc untouched). User CS= priority: addCommandToQueue() notes every non-own CS= while SAT is off (satNoteExternalCS); >= 8 C counts 130 s, < 8 C until CS=0; SAT or a user CS= taking over ends the guard without CS=0. Waits 60 s after boot, off in monitor mode, while flashing and while SAT's hand-back is pending. Own state, outside SAT's CS cache. Status: /api/v2/sat/status frost_guard_active (openapi.yaml). Thresholds fixed 6.0/7.0 (Q-460a open). ADR-294 Proposed. Known limits: a port 25238 CS= on the Classic is invisible to the guard (ADR-217); a user CS=0 while the guard is on is overruled by the next repeat.

Tests: tests/test_sat_frost_guard.py 13 OK on the fix; on origin/dev code 9 errors + 2 failures (block absent). Also fixed: test_ha_climate_sat_follow and test_ha_sat_sim_availability failed to compile ON origin/dev since my TASK-3011 push (sat/room_source moved into the core set; their stubs lacked iRoomSource / satPrimaryRoomSourceName); test_pic_pm_chain needed a satNoteExternalCS stub; test_otd_loop_task_commands' PIC-path comparison leaves the TASK-3021 hook out. Related tests OK: test_otd_loop_task_commands, test_pic_pm_chain (40), test_rest_command_sat_refusal, test_sat_curve_selftune, test_sat_diag_events (12), test_sat_immergas_tp, test_sat_no_return_startup, test_sat_ble_pin_only, test_sat_status_live, test_sat_humidity_source, test_adr_governance; evaluate --quick 0 FAIL.

2026-11-25 otgw-dev: entry 485 (Sergeant D, Q-460a answered A, 2026-11-25 06:03 UTC) closes the open point: the frost guard threshold is FIXED at 6 C on, 7 C off, NO setting, release 2.0.0. Together with entries 460 (Robert, Q-456a B: no thermostat connected, room <= 6 C gives a 30 C flow until 7 C) and 461 (Robert, DM B: both boards, Classic via the PIC: CS=30 + CH=1 through addCommandToQueue, release CS=0 at 7 C). Register scope: SAT off, no thermostat seen, room <= 6 C, 30 C flow, until 7 C, no setting, both boards.

Implementation choices, NOT in the register (ours, each marked as such in the code comment above the thresholds in SATcontrol.ino and in ADR-294): (1) monitor mode does nothing; (2) 60 s grace after boot for a thermostat to show itself; (3) a user CS= of 8 C or more wins for 130 s, a smaller one until a CS=0; (4) no action while a hand-back is pending or while flashing; (5) evaluation every 5 s; (6) CS=30 repeated about every 30 s (SAT_CS_REPEAT_MS); (7) the room chain.

Room chain (judgement, otgw-dev 2026-11-25): cut to the chosen BLE sensor and the external MQTT sensor only (entries 457/458; an explicit primary of one of them first). Removed: averaged areas and the thermostat MsgID 24. Why: the guard only acts with no thermostat on the bus, so MsgID 24 is by construction absent or stale; an average over areas can hide a cold room (a frost guard on a wrong room is the one failure that matters); the develop reference itself uses the primary room or the coldest room (demand_control.py:610-613), not an average. Cost: a unit whose room comes only from HA areas has no frost cover until it also gets a BLE or external sensor.

Reference quoted only as the entry does, opened this session at other-projects/SAT-develop (local copy HEAD 2b01ba65; entry 460 cites ba515c8): demand_control.py:610-622 _update_frost_protection (threshold <= 0 or no temperature -> off; <= threshold -> on; >= threshold + FROST_PROTECTION_HYSTERESIS_CELSIUS -> off), :815-828 (HVAC off and frost -> demand with requested_setpoint max(minimum_setpoint, FROST_PROTECTION_SETPOINT)), const.py:31-32 (hysteresis 1.0, setpoint 30.0), const.py:182 default 6.0. Alex has a threshold setting; we do not (entry 485). Reference only (ADR-292). Lane: cherry-pick of dcf09a698 and 8cc21d96e onto origin/dev 34cce5132, banner conflicts resolved with origin/dev's, own bump.

R1 bench .63 (bench-tester, cand-0827 bfae2b808 chain, 2026-11-25): frost steps 1-5 PASS on the PIC path (frost.md in OTGW-field-data/bench-63/2026-11-25_cand-0827): CS=30.0 + CH=1 at room 5.0, hold at 6.5, off at 7.0, stale room temperature -> off. AC2 (thermostat connected) NOT testable on the bench (N3), AC4 OT-Direct master mode not benched: both stay open.

2026-11-27 closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed. Status closed from backlog evidence; no new test run in this step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 and AC3 were ticked earlier. AC2: host test test_thermostat_connected_no_action in tests/test_sat_frost_guard.py (ae26b875a, first tag beta.438). AC4: host test red on the old code, green on the fix; PIC bench PASS (bench-63/2026-11-25_cand-0827/frost.md steps 1-5 and N1, N2, N4). The OT-Direct master-mode bench was never run (.61 offline) and is waived: the guard calls the shared decide function and addCommandToQueue. The bench cannot inject a thermostat. Per Robert's waiver: closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed.
<!-- SECTION:FINAL_SUMMARY:END -->
