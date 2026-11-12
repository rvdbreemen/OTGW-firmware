---
id: TASK-1857
title: >-
  OT-Direct loopback: SAT detects a manufacturer (and heating source) from the
  synthetic MsgID 3
status: To Do
assignee: []
created_date: '2026-11-12T03:07:00Z'
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
- [ ] #1 Decision recorded: loopback answers either leave manufacturer and heating-source detection alone, or deliberately stand in for a bench boiler (documented)
- [ ] #2 If they leave it alone: a host test feeds a loopback MsgID 3 and a real one through the real decode path; red on the old code (loopback sets iDetectedManufacturer), green on the fix, and the real frame still sets it
- [ ] #3 Bench on .61 by the bench-tester: loopback boot shows no 'Detected manufacturer' line from synthetic data (or the documented stand-in)
<!-- AC:END -->
