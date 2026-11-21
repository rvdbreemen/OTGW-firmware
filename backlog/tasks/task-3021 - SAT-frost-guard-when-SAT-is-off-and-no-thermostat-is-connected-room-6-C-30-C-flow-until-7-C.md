---
id: TASK-3021
title: >-
  SAT: frost guard when SAT is off and no thermostat is connected (room <= 6 C
  -> 30 C flow until 7 C)
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-21T18:47:00Z'
updated_date: '2026-11-21T18:55:00Z'
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
- [ ] #1 With SAT off, no thermostat detected and room <= 6.0 C, the gateway sends CH enable on with a 30 C flow setpoint; at room >= 7.0 C it returns to CH off
- [ ] #2 With a thermostat connected, behaviour is unchanged (hand-back only)
- [ ] #3 No room temperature known -> no frost demand
- [ ] #4 Host test on the real code: red on the old code (no demand at 5 C), green on the fix; bench or simulation check on OT-Direct master mode
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
<!-- SECTION:NOTES:END -->
