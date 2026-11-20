---
id: TASK-1751
title: >-
  OT-Direct master mode: TT=0 also clears an MQTT room_setpoint or BS= write on
  MsgID 16
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T09:29:00Z'
updated_date: '2026-11-16T19:33:00Z'
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
- [x] #2 Master mode on the wire: BS= alone writes MsgID 16 (once, then the 15 s schedule); TT=0 writes the remaining BS= or MQTT room_setpoint value at once; with nothing left no further WRITE 16; BS= / BS=0 / an MQTT room_setpoint during TT=/TC= leave TT's writes alone; gateway mode unchanged; host test red on the old code, bench by 09 on .61 master mode
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
2026-11-16 (95) AC1 was ticked on the state only; the wire part is the new AC. Decision (with the advisor, against the PIC first): the PIC has no master-mode MsgID 16 at all (standalone table gateway.asm 6.6 :1818-1830 has no ID 16; BS= only rewrites a thermostat's frames, roomsetptchange :3019-3024), so master-mode TT= writing MsgID 16 is already an OT-Direct extension; the defect is that state and wire disagree (overrides.write {16, 0x1300} while the boiler keeps TT's 20.50). Rule: in master mode one MsgID 16 write slot, TT=/TC= while active, else the BS= / MQTT room_setpoint value; the 15 s schedule sends the value in force. ADR-223 is gateway mode only (its Decision Contract 'TT=0 ... a BS= or MQTT room setpoint ... survives' still holds); ADR-241 and ADR-066 do not state BS= behaviour in master mode, so no ADR change.

Fix (OTDirect.ino): BS= in master mode stores the room value (otdRoom16Cache/Ovr) and, without a running TT/TC, enqueues WRITE 16 (write cache + one frame); BS=0 during TT clears only the room value (TT keeps valueSet); otdMqttSetRoomSetpoint() during TT in master mode only stores; clearRemoteOverride() in master mode enqueues the remaining room value at once (falls back to the schedule cache when the queue is full). Gateway mode unchanged. Tests: tests/test_otd_tt_pic_parity.py new MasterWire16Cases W1-W6 + R2 updated: FIX 52/52 OK; OLD (HEAD e18ccbfe3 OTDirect.ino + OTDirecttypes.h) 6 FAIL (R2, W1 BS alone no write, W2/W3 TT=0 writes nothing, W5 BS=0 during TT stops TT's write, W6 room setpoint written over TT); W4 (nothing set, no write) holds on both. Harness: test_otd_pic_float PRE gets OTDirectMode/otCurrentMode (gateway) and otRemoteOverride stubs (relay_rw now uses them). Full OTD sweep tests/test_otd_*.py (65 files) all OK; evaluate --quick 0 failed. Open: build + bench 09 on .61 master mode (BS=19 -> WRITE 16 0x1300; TT=20.5 -> 0x1480; TT=0 -> WRITE 16 0x1300 at once, then every 15 s).

Bench 09 on .61 (OTGW32, OT-Direct master, no boiler, SAT off), 2.0.0-beta.353+3a14423, run 2 15:2x (run 1 void: LAN broker down), commands via MQTT, frames from telnet (09 scratchpad b353\run2\): sha256 match, githash 3a14423, settings 'changed: none', bootcount 874 -> 876 (OTA only). (1) BS=19 -> 'R90101300 16 Write-Data TrSet = 19.00' at +1.6 s, overrides [16=4864]; (2) TT=20.5 -> R90101480 at +1.8 s, WRITE 100 = 3 at +2.9 s; (3) TT=0 -> R90101300 at +1.9 s, WRITE 100 = 0 at +3.0 s (beta.341: nothing in 360 s) = old-vs-fix PASS; (4) BS=0 -> no MsgID 16 frame in 90 s, overrides []. The 15 s refresh was not seen on the bench: with no boiler the bus is offline and the scheduler sends only MsgID 0 (OTDirect.ino 'if (busOffline && entry.msgId != 0) continue;', the round-robin after the TASK-1802 comment); queued commands still go out (the queue branch before it). The refresh therefore needs an answering boiler: covered by the host test only (W1: schedule entry valueSet with 0x1300; W5: TT keeps the schedule value), not benched.

AC2 is left for 09 to tick (bench AC, rule: the bench-tester ticks); periodic refresh host-only, as noted above.

Bench 09 on .61, beta.356+4601510 (code of pushed cf29dd198), master mode, SAT off, sim false: BS=19 -> R90101300 at +1.7 s; TT=20.5 -> R90101480 +2.0 s, R10640003 +3.1 s; TT=0 -> R90101300 +1.9 s, R10640000 +3.0 s; BS=0 -> no MsgID 16; identical to 353. Pushed cf29dd198, posted 1571965973295009792; 09 ticks AC2 (periodic refresh host-only).

AC2 ticked by 09 (2026-11-16). Bench on the wire, .61 OT-Direct master, SAT off, sim false, on 353+3a14423 (15:20) and 356+4601510 (19:35), identical results. BS=19 -> one R90101300 at +1.6/+1.7 s. TT=20.5 -> R90101480, then WRITE 100 = 3. TT=0 -> R90101300 at once (remaining BS value), then WRITE 100 = 0 (on 341 nothing was sent in 360 s). BS=0 -> no further WRITE 16 in 90 s. Covered by the HOST TEST only, not on this bench: the 15 s refresh schedule (the bus is offline without a boiler; OTDirect.ino:2299 skips everything but MsgID 0); BS=/BS=0/MQTT room_setpoint during TT=/TC=; and gateway mode unchanged. Evidence: 09 scratchpad b353/run2/, b356/bs/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT-Direct master mode: one MsgID 16 write slot, TT=/TC= while active, else the BS= / MQTT room setpoint. AC1: state test (TT=0 leaves the room value). AC2 (09 ticked): host test MasterWire16Cases W1-W6 + R2, fix 52/52, old 6 FAIL; bench .61 on 353 and 356 (= pushed cf29dd198): BS=19 -> R90101300, TT=20.5 -> R90101480 + WRITE 100=3, TT=0 -> R90101300 at once + WRITE 100=0 (old 341: nothing in 360 s), BS=0 -> no MsgID 16. Host-only (09's note): the 15 s schedule refresh (the bench bus is offline, scheduler sends only MsgID 0), BS=/BS=0 during TT=/TC=, gateway mode unchanged. PIC has no master-mode MsgID 16 (gateway.asm 6.6 :1818-1830); ADR-223 is gateway mode only, no ADR change. Pushed cf29dd198, posted 1571965973295009792.
<!-- SECTION:FINAL_SUMMARY:END -->
