---
id: TASK-1846
title: 'OT-Direct: PS=1 does not show the OT= value in the MsgID 27 field'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:59:00Z'
updated_date: '2026-11-12T09:06:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 925000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the PIC 6.8 parity audit (N12), docs/reviews/2026-11-12_pic68-otdirect-parity-audit/REVIEW.md. G: = gateway.asm 6.8 (OTGW-other-projects/otgw-6.8, 770aa45); O: = src/OTGW-firmware/OTDirect.ino at fa5790118. Read from code and the PIC's own test scripts only; not reproduced on a bench.

- PIC: OT= writes the value straight into the summary store (StoreOutTemp G:4276-4287, slot G:568-569, index 15 = MsgID 27).
- OT-Direct: store filled only from frames (T WRITE, B READ_ACK, R WRITE without thermostat; O:2097-2106). With a thermostat connected OT=7.5 never reaches field 16 (O:3141-3153). Rest of the line matches the PIC print routines.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 host test: thermostat connected, OT=7.5 then PS=1 -> field 16 = 7.50; red on current code
- [x] #2 line stays byte-identical to tests/summary-1.stc for the same frames
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Not verified (from the audit): for an ID with an SR= entry the PIC still stores the boiler's answer while OT-Direct sends no boiler frame (TASK-1707 note b), so that field can differ too.

Part of the PIC 6.8 parity audit, parent TASK-1851.

Implemented (lane-1846, local; release window, no push yet). PIC 6.8: SetOutsideT (gateway.asm :4256-4266) -> StoreOutTemp (:4276-4287) writes a valid OT= value to outside1/2 and to valuestorage+30 (outsideval1, :568-569) = the MsgID 27 summary slot, with or without a thermostat; OT=- only clears the OutsideTemp flag (ClrOutsideT :4262), the slot keeps its value. OTDirect.ino: new otdPsStoreOutside() fills slot 27 when OT= is accepted. Second parity fix in the same branch: OT=- passed as a number (value[0] == '-') and wrote 0.00 to the boiler and the slot; now only a digit or a '-'/'+' followed by a digit is a value (PIC GetFloatArg/GetFloat :5118-5129: after the sign GetDecimal fails on an empty string -> ClrOutsideT). Evidence: new tests/test_otd_ps1_outside_temp.py, real PS=1 block + PS=/OT= branches + loop step on the stubs of test_otd_ps1_summary_pic: fix 7 OK; pre-1846 OTDirect.ino 6 failures (field 16 stays 0.00; OT=- wrote a third MsgID 27 write). AC2: the frames of the PIC's own tests/summary-1.stc with the replies of tests/boiler.stc give the stc's expected line byte for byte on the old code and on the fix; after OT=7.5 only field 16 changes (7.50). Assumption, stated in the test: boiler.stc holds one data word per DataID and the simulator answers a READ with Read-Ack (the table words for 56/57 are Write-Ack while the expected line has 60.00/50.00, which the PIC stores only from a Read-Ack, HandleResponse :2438-2439). Regression: 26 OT-Direct test files pass, test_otd_ps1_summary_pic 9 OK, test/host/test_override_reply OK, evaluate --quick 86/0.

Shipped 2026-11-12: origin/dev a4b53fde9..89a37c700 (beta.209 TASK-1846, beta.210 TASK-1855, beta.211 comment), zip 2.0.0-beta.211+89a37c7 posted #alpha-testing msg 1570364042554376216 (rebuilt after the rebase onto ba's docs; src identical to the benched 211+cf7c808). Before the build: 36/36 targeted test files, test_override_reply OK, evaluate --quick 86/0. Bench by 09 on .61, 211+cf7c808: OTA app+fs, bootcount 457->459, health 200, MQTT, crashlog none, 164 settings unchanged; restored afterwards (settings diff 0, reboot, bootcount 464). Logs: 09 scratchpad b211/ (bench12-A.log, bench12.log). TASK-1846 in master mode (otdmode 3, satsimulation off, port 25238 on, temporarily): '09:46:20.856 OT: 7.50', one WRITE 'R901B0780' (MsgID 27 = 7.50); PS=1 at 09:46:23.812 field 16 = 7.50; '09:46:26.790 OT: -'; second PS=1 at 09:46:29.818 field 16 still 7.50; up to 09:46:50 no R frame for MsgID 27 after OT=- (no 0.00 write).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OT= now stores its value in the PS=1 summary slot of MsgID 27 (PIC SetOutsideT -> StoreOutTemp, gateway.asm 6.8 :4256-4287), with or without a thermostat; OT=- clears like the PIC (GetFloat :5118-5129, ClrOutsideT) instead of writing 0.00. AC1: tests/test_otd_ps1_outside_temp.py on the real PS=1 block and PS=/OT= branches, 7 OK, 6 red on the old code. AC2: the PIC's own tests/summary-1.stc frames with tests/boiler.stc replies give the stc's expected line byte for byte before and after the fix (assumption on the simulator's reply type stated in the test). Bench 09 on .61 211+cf7c808: field 16 = 7.50 after OT=7.5, kept after OT=-, no MsgID 27 write after OT=-. Shipped beta.209 (89a37c700, zip msg 1570364042554376216).
<!-- SECTION:FINAL_SUMMARY:END -->
