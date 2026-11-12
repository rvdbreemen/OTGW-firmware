---
id: TASK-1831
title: >-
  OT-Direct: VS= writes the ventilation setpoint in the wrong byte, once, and
  never overrides the thermostat's MsgID 71
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:37:00Z'
updated_date: '2026-11-12T02:37:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 914000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N1), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: VS= stores the value (G:4241-4254); every thermostat MsgID 71 request becomes a WRITE, value in LB, HB 0 (MessageID71 G:3210-3217). tests/ventilation-1.stc: VS=15 -> R9047000F; VS=XX -> `VS: -`. Spec v4.2:1514 "LB: Control Setpoint".
- OT-Direct: `data = setpt << 8` (HB) (O:3168; boot O:1019; OTDirecttypes.h:124 comment "MsgID 71 HB"). Own decoder reads LB (OTGW-Core.ino:5425). Goes out once via the queue; no refresh, no override of the thermostat frame.
- Consequence: VS=50 sends 0x3200, read as 0 %. A thermostat writing MsgID 71 overrules the command.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test on the real handler: VS=15 -> data 0x000F; red on current code
- [x] #2 With VS held, thermostat MsgID 71 READ/WRITE goes to the boiler as WRITE with the VS value; VS=T restores pass-through and replies `VS: -`
- [x] #3 Persisted setpoint applied after boot in gateway and master mode (closes TASK-1760)
- [x] #4 Bench capture on the OT-Direct unit shows R.047.... with the value in LB
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): behaviour of a real ventilation unit.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Ground truth checked in this session: spec v4.2 MsgID 71 'LB: Control Setpoint ventilation / heat-recovery' u8 0..100 (docs/opentherm specification/...v4.2.md:1514-1518); PIC 6.8 MessageID71 (gateway.asm:3210-3217) sets MsgWriteOp, value via setbyte4 (LB), HB cleared; SetVentSetpoint (4241-4254) non-numeric -> ValueCleared; tests/ventilation-1.stc VS=15 -> R9047000F, VS=XX -> 'VS: -'. Fix: VS= data in LB; clear replies 'VS: -'; MsgID 71 gets a W_ENTRY and an otOverrides slot (both missing, so override and boot restore were no-ops, closes the TASK-1760 premise); applyOverrides turns a thermostat READ 71 into WRITE 71 with the VS value while VS is held; boot restore writes the LB value; OTDirecttypes.h comment says LB. Not changed: VS>100 is still clamped (PIC answers OutOfRange) - left for TASK-1804's range-check item.

Test tests/test_otd_vs_msgid71.py (real VS branch, enqueueWriteCommand, applyOverrides, set/clearOverride, updateWriteCache, clearWriteOverride, otOverrides table and the boot-restore block cut from OTDirect.ino): old code 6 FAIL, fix 6 OK (VS=15 sends 9047000F; thermostat WRITE 90470014 -> 9047000F as ventilation-1.stc; thermostat READ 71 -> WRITE 9047000F; VS=XX -> 'VS: -' and pass-through; persisted 40 % -> cache 0x0028 and the override after boot; the real schedule has W_ENTRY(71)). Neighbours OK: msgid99 6, tt_pic_parity 40, pr_reports 4, ps1_summary 9, gateway_alt_slots 12. evaluate --quick 92 checks 0 FAIL.

Deliberate deviation (recorded, not PIC): with W_ENTRY(71) and a VS value held, gateway mode also sends its own WRITE 71 in alternative slots when the thermostat does not write 71, and the persisted VS is written after every boot. The PIC only rewrites the thermostat's frames (or sends 71 with AA=71). Same model as the existing write entries 1/7/8/14/16/24/27/56/57 (TASK-1707 note a, alternative slots from the whole schedule, listed as an intentional difference in the 6.8 audit section 5). Needed for AC3 in master mode, where no thermostat frame exists. Bench on .61: loopback logs the gateway's R frame and the simulated B to the parser (sendMasterRequestAsync loopback branch), so R.047.... with the value in LB is visible there.

Integrated and built: rebased on ac5967ad2, one bump per task (1823 beta.191, 1828 192, 1830 193, 1832 194, 1831 195, 1833 196, 1845 197) + test commit 4cafc97eb; full run_tests.py 208/210 (test_override_reply fixed in 4cafc97eb, test_stream_real_cbuf -j4-only, unrelated); evaluate --quick 93 checks 0 FAIL; build.bat esp32-combo 'Build completed successfully', build/OTGW-firmware-esp32-combo-2.0.0-beta.197+4cafc97-flash.zip (6158.3 KB). Bench requested from 09 via the coordinator.

Bench (otgw-bench-tester 09, .61 loopback, commands via TCP 25238, telnet + MQTT captured; beta.197+4cafc97 vs 190+1a4c0a4): flash OK, githash 4cafc97, health 200. Pushed ac5967ad2..4cafc97eb, zip posted (Discord 1570265549747781763). TASK-1831 PASS: VS=15 old R90470F00 -> new R9047000F, reply 'VS: 15'; VS=x 'VS: x' -> 'VS: -'. 09 saw the WRITE twice on the socket: real double send from the existing enqueue+schedule pattern of every write command (now visible for 71 because of its new write entry), filed as TASK-1855 (low).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 host test: 6 FAIL old / 6 OK fix (frame 9047000F as ventilation-1.stc). AC2 thermostat READ/WRITE 71 -> WRITE with the VS value, VS=x -> 'VS: -': host test + 09 reply check. AC3 persisted setpoint after boot (gateway + master): host test of the real boot-restore block with the new write entry/override slot. AC4 bench R.047.... with the value in LB: 09 R9047000F. Deviation recorded: own WRITE 71 in alternative slots (write-entry model). Follow-up TASK-1855 (double WRITE). Commit 1d4b8fdac (beta.195), pushed, posted.
<!-- SECTION:FINAL_SUMMARY:END -->
