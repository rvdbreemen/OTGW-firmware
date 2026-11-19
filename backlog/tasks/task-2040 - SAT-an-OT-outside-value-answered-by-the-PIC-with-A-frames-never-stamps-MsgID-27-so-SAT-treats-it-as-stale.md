---
id: TASK-2040
title: >-
  SAT: an OT= outside value answered by the PIC with A frames never stamps MsgID
  27, so SAT treats it as stale
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T01:20:00Z'
updated_date: '2026-11-19T03:02:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1046000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implementation gap against entries 330 (Robert, 2026-11-13) and 331 (Sergeant D agrees): an outside temperature the gateway itself puts on the bus as MsgID 27 (PIC DS18B20 TS=O, or OT=) counts as a SAT outside source at the MsgID-27 rank of entry 139 (external, MsgID 27, weather, restored), 1 h validity (entry 57). otFrameIsFreshOutsideTemp() (OTGW-Core.ino:5241-5247, TASK-1896) stamps only a boiler Read-Ack 27 (not substituted, not replayed) and the gateway's own R Write-Data 27; it never stamps an A frame (rationale: an OT-Direct cache answer can repeat an old value). Field evidence: marceld (beta.361, capture 20261117-081252-upload, hours 08-10): HA sends OT= (set/<node>/outside, MQTTstuff.ino:421/512), and all 230 MsgID 27 frames are T (thermostat Read-Data) + A (gateway Read-Ack with the OT= value, e.g. A401B0558 = 5.34 C), no B and no R; so the stamp never refreshes, SAT ran on the weather (and before TASK-2039 handed the boiler back when the single stamp expired). Entry 330's PIC note expects MessageID27 to turn the Read-Data into a Write-Data towards the boiler (gateway.asm 6.6:3050-3067 UserOutsideTemp); why marceld's PIC answers with A and sends no R (e.g. the boiler does not support 27: MandatoryID / AlternativeUsed at :3053) is to be checked. Fix direction: on a PIC board, stamp an A answer 27 to the thermostat whose value is the gateway's own outside value (OT=/TS=O), and keep excluding the OT-Direct cache answer. Note: like the R path, the PIC repeats an OT= value until it is changed or cleared, so freshness follows the frames, not the age of the HA value.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Red host test on the real otFrameIsFreshOutsideTemp()/stamp path: marceld's T+A sequence leaves MsgID 27 unstamped on the current code
- [x] #2 Fix: the PIC's A answer with the gateway's own outside value stamps MsgID 27 (SAT source O); the OT-Direct cache answer and replayed/parity/PS=1 frames stay excluded; test green
- [ ] #3 Bench or replay: a PIC unit fed with OT= keeps SAT on source O (osrc=O) with no hand-back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 fix (lane-2040, on top of TASK-2039 7a9829718): otFrameIsFreshOutsideTemp() (OTGW-Core.ino) also stamps an A Read-Ack 27 that is not an answer-override (the PIC's proxy answer with its OT=/TS=O value). OT-Direct is unaffected: every A it makes goes through bridgeFrameToParser() -> enqueueOTFrame(..., OTFRAME_SRC_OTDIRECT_LOCAL) (OTDirect.ino:1166-1167, the only enqueue there), so it is bLocalAnswer and rejected first; the master-mode cache answer (handleMasterModeSlaveFrame) included. Same split as the canonical value: a proxy A reaches canonical Toutside, an answer-override A does not (ADR-103, is_value_valid_for_master_topic, OTGW-Core.ino:2042-2044), so SAT's value and stamp now come from the same frame. Red/green: tests/test_outside_stamp_gateway_frames.py test_pic_proxy_answer_with_the_gateway_value_stamps FAILS on the code before (1 failed, 11 passed), passes on the fix; new test_otdirect_answers_and_invalid_answers_do_not_stamp (local A, Data-Invalid, replayed, parity: no stamp); related files 57 passed. Open: why marceld's PIC answers with A and sends no R (gateway.asm 6.6:3053 MandatoryID/AlternativeUsed?); not needed for the fix.

2026-11-19 AC3 is a field check (coordinator): .63 and .61 have no thermostat on their OT bus, and the PIC's proxy A only appears as an answer to a thermostat Read-Data 27; a replay does not count (bReplayed). Field: marceld on 381 or later (signature: SATTICK osrc=O while HA sends OT=, no 'no outside temperature younger than 1 h' line). 381 on .63 = regression only.

2026-11-19 bench (09, .63, 382+e562f82 which contains 381, regression only, no thermostat): 30 min no restart (bootcount 233), heap flat, OTA 'changed: none'. One 1.1 s loop-stall (step 4->5) came from 09's visit to the Advanced page (deferred PIC update check + firmware/files, 655 ms), not from TASK-2040. The feature (the PIC proxy A 27 with the OT= value stamps MsgID 27) cannot be benched without a thermostat: AC3 = field (marceld on 381+).
<!-- SECTION:NOTES:END -->
