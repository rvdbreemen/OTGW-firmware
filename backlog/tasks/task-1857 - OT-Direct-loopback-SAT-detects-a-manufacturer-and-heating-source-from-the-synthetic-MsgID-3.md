---
id: TASK-1857
title: >-
  OT-Direct loopback: SAT detects a manufacturer (and heating source) from the
  synthetic MsgID 3
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-12T03:07:00Z'
updated_date: '2026-11-15T00:22:00Z'
labels:
  - otdirect
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 934000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of TASK-1856 (bench-tester run 2 on .61, beta.197, telnet 03:49:49.319): in OT-Direct loopback SAT logs 'satDetectMan(284): SAT: Detected manufacturer: Remeha (MemberID 11)' from the synthetic MsgID 3 answer. processOT() decodes every MsgID 3 frame, also a loopback 'B' that bridgeFrameToParser() marks localAnswer, and calls satDetectManufacturer(OTdata.valueLB) (OTGW-Core.ino:3225, SATcontrol.ino:275-288) and sets state.sat.iDetectedHeatingSource from HB bit 2 (OTGW-Core.ino:3223). The detected manufacturer feeds SAT's per-brand handling (for example the MI=500 brands in thermo-nova's boot, and the Immergas TP), so a bench in loopback runs SAT as if a Remeha were connected. SAT-adjacent detection, not a SAT behaviour change. The TASK-1856 marker idea (who stored MsgID 3: real boiler or loopback) may apply. Open question first: is the loopback MemberID meant to stand in for a real boiler on the bench (then document it) or should loopback leave the detection alone (then fix it)? Not reproduced beyond that one log line.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision recorded: loopback answers either leave manufacturer and heating-source detection alone, or deliberately stand in for a bench boiler (documented)
- [x] #2 If they leave it alone: a host test feeds a loopback MsgID 3 and a real one through the real decode path; red on the old code (loopback sets iDetectedManufacturer), green on the fix, and the real frame still sets it
- [x] #3 Bench on .61 by the bench-tester: loopback boot shows no 'Detected manufacturer' line from synthetic data (or the documented stand-in)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Decision (coordinator, 2026-11-12, on behalf of the 2.0.0 coordination): loopback frames must not drive detection, neither the manufacturer (satDetectManufacturer) nor state.sat.iDetectedHeatingSource. Same principle as TASK-1856: synthetic answers are not evidence about a real boiler. A bench that needs a fake brand sets it explicitly, not via loopback. So AC1 is settled as 'leave detection alone'; AC2 (red/green host test) and AC3 (bench) remain. Status stays To Do, priority low.

2026-11-14 (92, lane wt-92-webui; TASK-1903 item 1 done in the same change, agreed with d3). New host test test/host/test_slave_config_evidence.py slices the REAL print_slavememberid() from OTGW-Core.ino (MSVC). --rev HEAD (e74bb6ed0, before the fix): FAIL 6 = an OT-Direct loopback B Read-Ack MsgID 3 (bLocalAnswer) and a replayed one (bReplayed) each call satNoteSlaveConfig() once, satDetectManufacturer() once and set iDetectedHeatingSource 2 (heat pump from HB bit 2). Fix (boilerEvidence = !bLocalAnswer && !bReplayed in front of all three): 9/9 PASS, a real boiler Read-Ack still sets all three. Evidence: comms scratchpad t1857/old.txt, new.txt.

AC3 ticked by 09 with the documented stand-in: 09 bench 2026-11-15 on .61 (OTGW32), 2.0.0-beta.271+7ecd150 (OTA app+FS, changed none, bootcount 697->699), headless Chrome over CDP (scratchpad tools/uishot.mjs), screenshots in 09 scratchpad b271/ with IP/hostname scrubbed. A loopback BOOT is not reachable on .61 (otdautodetect on, GW=L is not persisted), so runtime GW=L, which drives the same decode path (print_slavememberid on every MsgID 3). Telnet: 'OTD: mode -> loopback (simulated boiler responses)', then the synthetic 'Boiler B4003000B 3 Read-Ack > SlaveConfigMemberIDcode ... MemberID 11'. /api/v2/sat/status before, during (55 s) and after: slave_memberid 0, manufacturer_detected 18 (Other), heating_source_detected 1, slave_cfg_valid false, unchanged; 0 'Detected manufacturer' lines (weak alone: the SAT debug flag may have been off; the status fields are the evidence). Back to GW=S (PR: M=S). Evidence b271/i6/tel.raw.
<!-- SECTION:NOTES:END -->
