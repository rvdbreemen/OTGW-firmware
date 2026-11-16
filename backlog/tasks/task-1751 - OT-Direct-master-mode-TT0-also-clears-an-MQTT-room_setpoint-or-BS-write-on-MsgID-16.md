---
id: TASK-1751
title: >-
  OT-Direct master mode: TT=0 also clears an MQTT room_setpoint or BS= write on
  MsgID 16
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-09T09:29:00Z'
updated_date: '2026-11-16T14:31:00Z'
labels:
  - otdirect
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 853000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-1733 / ADR-223 (found by the 2026-11-09 correctness review, otgw-firmware-80). In OTD_MODE_MASTER, clearRemoteOverride() still calls clearWriteOverride(16) and (100) (OTDirect.ino ~2389-2391), which resets the MsgID 16 write cache (W_ENTRY(16), schedule ~164) and the shared slot-16 override, so TT=0 also drops an MQTT room_setpoint or BS= value. Older behaviour kept by ADR-223 for master mode; decide whether TT=0 should clear only what TT= set.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 TT=0 in master mode leaves an MQTT room_setpoint / BS= value on MsgID 16 untouched (or Robert decides the current behaviour stays, recorded in the notes); host test
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision (Robert, DM 1571426506259963986, reply to the coordinator triage question 5/5 DM 1571283755438448772: "TASK-1750 + TASK-1751 (OT-Direct): MsgID 100 remote-override flags like the PIC (TT b'11', OH= high byte), and TT=0 also clears an MQTT room_setpoint/BS= on MsgID 16. A) both PIC parity, consistent with your 'like the PIC' on 1806. B) keep ADR-223 as is. Recommendation: A"): "A". Interpretation: A = PIC parity. The PIC has no BS= command and no MQTT room setpoint (command list gateway.asm:45-145 has TT/TC but no BS); nothing but TT/TC sets a room setpoint value. TT=0 goes SetTempSetPoint :4088 -> SetSetPoint :4096, zero value -> ClearSetPoint :4144-4153, which touches only the override state (OverrideClr, ManChangePrio/ProgChangePrio, or clrf override) and the setpoint1/2 vars; MessageID16 :2984-3040 then only acts on that override state. So on the PIC, TT=0 clears exactly what TT= set and nothing else. Parity therefore = AC1 fix: in OT-Direct master mode TT=0 leaves an MQTT room_setpoint / BS= write override on MsgID 16 untouched. Master mode has no PIC counterpart (no thermostat), so this is parity by the PIC rule 'TT=0 clears only the TT state'. ADR-223 needs no amendment: its Must already says 'TT=0 and a release clear only the remote override state; a BS= or MQTT room setpoint write override on MsgID 16 survives' without a mode limit, and its master-mode Exception covers only the TrSet writes of TT=/TC=.

CORRECTION to the note above (my error): the PIC DOES have BS=: dispatch gateway.asm:3900 -> SetFakeSetpoint :4289-4298 stores fakesetpoint1/2 (NoFakeSetpoint when 0), and MessageID16 roomsetptchange :3027-3033 writes that value into the thermostat's MsgID 16. The conclusion stands and is now firmer: fakesetpoint1/2 and NoFakeSetpoint are written only at :1287 (init) and :4292-4297 (BS=); TT=0 (SetTempSetPoint :4089 -> SetSetPoint :4096 -> ClearSetPoint :4144-4153) never touches them. So on the PIC TT=0 leaves a BS= value in effect -> parity = AC1 fix. OT-Direct cause: in master mode TT= writes the same MsgID 16 slot (enqueueWriteCommand -> updateWriteCache + setOverride, OTDirect.ino:2779-2795) as BS= (:3773-3787) and MQTT room_setpoint (:2583), and clearRemoteOverride() then calls clearWriteOverride(16) (:3039). Plan: remember the non-TT MsgID 16 value (BS= / MQTT room_setpoint) and on TT=0 / release in master mode put that value back into the slot instead of clearing it; clear only when none is set.

Implemented (lane-95-next): otdRoom16CacheSet/F88 (MQTT room_setpoint: periodic WRITE 16 value) and otdRoom16OvrSet/F88 (BS= or MQTT room_setpoint: frame override) kept by otdMqttSetRoomSetpoint() and the BS= branch (BS=0 clears both, as clearWriteOverride(16) did), reset in resetTransientState(); clearRemoteOverride() in master mode clears slot 16 and then puts those values back. Gateway mode unchanged (it already returned before touching slot 16). Evidence: tests/test_otd_tt_pic_parity.py MasterSlot16Cases on the real otdMqttSetRoomSetpoint/BS=/TT= code: R1 MQTT 19.0 -> TT=21.5 -> TT=0 leaves (write 0x1300, override 0x1300), R2 BS=19.0 -> TT=0 leaves override 0x1300, R3 nothing before -> slot cleared as before. On HEAD's OTDirect.ino R1 and R2 FAIL, R3 passes. Harness stubs for the new variables added to test_otd_pic_float, test_otd_mode_change_keeps_state, test_otd_mode_switch_queue_reset.

Landed beta.300: Pushed origin/dev 2e4ca9c05..3e29faa63; zip beta.301+2b3d9a2 posted (Discord 1571471374038470711); final build exit 0 'Build completed successfully', RAM 130572 B, Flash 2157483 B; sweep 88/88, evaluate --quick 86/0; bench 09 on .61 beta.301+2b3d9a2 PASS (smoke + re-check). Bench 09 on .61: state level only, the MsgID 16 override 0x1300 stays after TT=0 in master mode; no WRITE 16 frame seen within 75 s (the periodic write interval was not reached in the window). AC1 (TT=0 leaves room_setpoint/BS= untouched) holds on the state and in tests/test_otd_tt_pic_parity.py R1-R3 (R1/R2 FAIL on the old code); the frame on the bus is NOT shown yet -> In Review until a longer master-mode capture shows the WRITE 16 0x1300.

2026-11-16 10:34-10:43 bench 09 on .61 (OTGW32, master mode, no thermostat or boiler), 2.0.0-beta.341+3cb72c3, telnet captured after boot (1708 lines, 464 OT frames), commands over MQTT OTGW/set/<node>/command. BS=19 -> overrides.write {msgid 16, value 4864 = 0x1300}, no MsgID 16 frame in the next 70 s. TT=20.5 -> {16, 5248 = 0x1480} and at once a WRITE 16 frame R90101480 (20.50) 1.9 s after the command. TT=0 -> {16, 4864 = 0x1300}: the BS value is kept (AC1 holds at the state level, as in the beta.301 bench). After TT=0: 0 MsgID 16 frames in 360 s, so the restored 19.00 was not written to the boiler in that window; a boiler would keep the last written TrSet 20.50 until the next MsgID 16 write. BS=0 -> write list empty. Open question for the owner (behaviour, not ticked here): should TT=0 in master mode trigger an immediate WRITE 16 with the kept BS value, and is there a periodic MsgID 16 write in master mode at all (none seen in 70 s with BS alone or 6 min after TT=0)? Evidence: 09 scratchpad closeout/mq2/ (run.log, telnet.raw, bs1751.py).

Bench 2026-11-16 (otgw-firmware-09), .61 OT-Direct master, 2.0.0-beta.353+3a14423, SAT off, MQTT command topic, frames from telnet processOT. BS=19: R90101300 (TrSet 19.00) at +1.6 s. TT=20.5: R90101480 at +1.8 s, then WRITE 100 R10640003 at +2.9 s. TT=0: R90101300 at +1.9 s (back to the BS value), then WRITE 100 R10640000 at +3.0 s. On 341 nothing was sent in 360 s after TT=0, so this is the old-vs-fix proof. BS=0: no MsgID 16 frame in 90 s, overrides []. Smoke: githash 3a14423, bootcount 874 -> 876 (2 OTA restarts only), settings.ini identical, MQTT connected.

Periodic refresh host-only: each command gave exactly one WRITE 16 with no repeat in 70-120 s. Telnet does print repeated frames (122x R00000000 in the same run). The bus is offline (no boiler) and the scheduler skips everything but MsgID 0 then: origin/dev OTDirect.ino:2299 'if (busOffline && entry.msgId != 0) continue;' (opened by 09, confirmed by 95). The one-shot writes go through the command queue. Evidence: 09 scratchpad b353/run2/ (run.log, telnet.raw).
<!-- SECTION:NOTES:END -->
