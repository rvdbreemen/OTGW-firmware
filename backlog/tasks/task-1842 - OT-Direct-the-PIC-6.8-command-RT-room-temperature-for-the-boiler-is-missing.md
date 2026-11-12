---
id: TASK-1842
title: >-
  OT-Direct: the PIC 6.8 command RT= (room temperature for the boiler) is
  missing
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:53:00Z'
updated_date: '2026-11-12T10:31:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 922000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N9), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC 6.8: RT=<temp> stores a fake room temperature, 0 clears (G:3849-3856, :4300-4309, :372-374, :1288). Thermostat MsgID 24 gets that value (MessageID24 G:3061-3068); as alternative (AA=24) it goes out as WRITE (G:2670-2673). tests/heatpump-1.stc RT=25.41 -> R10181969. Reply `RT: 25.41`.
- OT-Direct: no RT branch -> NG (O:3911-3915); MsgID 24 has no override slot (O:434-442); MQTT room temperature path writes MsgID 24 as a gateway frame (O:2269-2273).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 RT=25.41 replies `RT: 25.41`; a thermostat WRITE 24 reaches the boiler with 0x1969; RT=0 stops it; host test red on current code
- [x] #2 thermostat gets WRITE_ACK with its own data
- [x] #3 OTD_PIC_COMPAT_VERSION (O:533) goes to 6.8 once this and the MsgID 125 part of N10 have landed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): for a thermostat frame with parity bit 1 the PIC appears to answer Unknown-DataID (setbyte1 G:2680-2686, SendAltResponse G:2467-2477); read from code only. OT-Direct chose not to copy this for 56/57 (O:1482-1488).

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implementation (wt-95rt, base origin/dev 61b69e037): RT= branch in handleOTDirectCommand (before C2), modelled on BS=: setOverride(24, f88) + updateWriteCache(24, f88), nothing sent at once; RT=0 -> clearWriteOverride(24) (PIC NoFakeRoomTemp). MsgID 24 added to otOverrides, so applyOverrides() puts the RT value into a thermostat WRITE 24 and buildOverriddenReply() answers WRITE_ACK with the thermostat's own data. Without a thermostat the schedule's W_ENTRY(24) sends it (master mode / alternative slots). Parser otdPicPercentF88() (markers) is the PIC's GetPercentArg: GetPosFloat + GetFloat (gateway.asm:5114-5185) + GetPercent (:5099-5112): two fraction digits, the third rounds, hundredths -> 1/256 rounded UP (:5169-5178); '-' = SE, above 100.00 = OR, other characters = SE. Reply prints the stored f8.8 with two decimals (PIC CommandFloat). floatToF88() was not used because it truncates (25.41 -> 0x1968 vs the PIC's 0x1969); that broader issue is TASK-1859.

Deliberate consequence: because MsgID 24 is now in otOverrides, the OT-Direct MQTT room_temp command (otdMqttSetRoomTemp -> enqueueWriteCommand(24) -> setOverride(24)) now also overrides a thermostat's WRITE 24, like RT=. Not covered: a thermostat READ of MsgID 24 is not turned into a WRITE (the PIC's MessageID24 + messagewrite would, but which answer the thermostat then gets is not settled from the code); thermostats write MsgID 24.

Evidence AC1/AC2: tests/test_otd_rt_room_temp.py (6 tests; real RT= branch, parser, otOverrides + setOverride/clearOverride, applyOverrides, buildOverriddenReply, updateWriteCache, clearWriteOverride, parity and buildOTResponse). Fix 6/6 OK: 'RT: 25.41' and cache 0x1969; thermostat WRITE 0x90181400 goes to the boiler as 0x10181969 (exactly heatpump-1.stc's R10181969); reply 0x50181400 (WRITE_ACK, own data); RT=0 -> 'RT: 0.00', cache cleared, frame unchanged; RT=x / RT=-1 -> SE, RT=100.01 -> OR; 0.02 -> 0x0006 (round up), 25.415 -> 'RT: 25.42' 0x196C. Old code (origin/dev 61b69e037): 5 FAIL (RT= is NG, nothing overridden), 1 ok (the unmodified echo). tests/test_otd_tt_pic_parity.py harness extended with the parser slice (it cuts the handler up to C2, which now contains RT=); green on lane and origin. All 39 tests that slice the touched functions rerun on this base: green. evaluate --quick 92 checks 0 failed. Manual ch10 RT= row added.

Open: AC3 (OTD_PIC_COMPAT_VERSION -> 6.8) waits for the MsgID 125 part of TASK-1844. Bench not run (.61 is in the public-beta soak); for the bench-tester: in loopback/master on .61, RT=25.41 -> loopback R frames WRITE 24 data 0x1969 (R10181969 after parity); RT=0 -> they stop.

USER-VISIBLE BEHAVIOUR CHANGE (for the alpha note): on OT-Direct, the MQTT command room_temp (topic <top>/set/<node>/otgw32/room_temp, MQTTstuff.ino:925-934, only active on OT-Direct hardware) now works like RT=: from that command on, a thermostat's own WRITE of MsgID 24 (room temperature) reaches the boiler with the MQTT value instead of the thermostat's, and the thermostat gets WRITE_ACK with its own value. Before, the value was only sent as the gateway's own MsgID 24 writes and the thermostat's writes passed unchanged. This is what RT= does on the PIC: MessageID24 puts fakeroomtemp into every thermostat MsgID 24 request while NoFakeRoomTemp is clear (OTGW-other-projects/otgw-6.8/gateway.asm:3061-3068), set by SetRoomTemp (:4300-4309). RT=0 (or a clear via RT=) gives the thermostat's value back. Public wording must not name the board (the topic segment is internal).

AC3: OTD_PIC_COMPAT_VERSION is 6.8 in the TASK-1844 commit (RT= here + the MsgID 125 clamp there); the PR=A banner is 'OpenTherm Gateway 6.8 OTGW32' (Robert's decision, TASK-1844 notes).

Pushed origin/dev e8af7d61a..5d03c17c3 (beta.212 TASK-1842, beta.213 TASK-1844, beta.214 TASK-1860, test-only 5d03c17c3); zip OTGW-firmware-esp32-combo-2.0.0-beta.214+5d03c17-flash.zip posted (Discord msg 1570385106462580746). Bench by otgw-bench-tester (09) on .61, beta.214+5d03c17 (zip sha256 e167f2af1e49b850...), OTA 211 -> 214, settings 164 keys unchanged; evidence in session 09 scratchpad b214\ (baseline-old.txt, post-PR.txt, pyotgw-read.txt, rt-set.txt, rt-clear.txt, regress-poll.log, regress-telnet.log). Old 211: PR=X no answer, PR=A 'OpenTherm Gateway 6.6', RT=21.50 'NG', no otdpiccompat. Regression 11 min: bootcount delta 0, no crashlog, MQTT and health 200 in 23/23 polls. TASK-1842: RT=21.50 -> 'RT: 21.50', /api/v2/otdirect/overrides write [{msgid 24, value 5504}] (0x1580 = 21.50), otdoverrides 1; RT=0 -> 'RT: 0.00', write [] and otdoverrides 0: PASS. Host-test only: the MsgID 24 frame to the boiler (no thermostat or slave sim on .61).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
RT= (PIC 6.8 GetPercentArg parsing: two fraction digits, third rounds, '-' = SE, above 100.00 = OR) sets a MsgID 24 write override that a thermostat WRITE 24 carries to the boiler; RT=0 clears it. AC1/AC2: tests/test_otd_rt_room_temp.py 6/6, 5 FAIL on the old code; the frame equals heatpump-1.stc R10181969. AC3: OTD_PIC_COMPAT_VERSION 6.8 with TASK-1844. Bench 09 .61 beta.214: RT= accepted, override set and cleared (old 211: 'NG'). Not covered on hardware: the MsgID 24 frame on the bus (no thermostat or slave sim); host-test evidence only. Pushed as beta.212 (8f6924552), zip posted. Follow-up TASK-1859 (floatToF88 truncates where the PIC rounds).
<!-- SECTION:FINAL_SUMMARY:END -->
