---
id: TASK-2040
title: >-
  SAT: an OT= outside value answered by the PIC with A frames never stamps MsgID
  27, so SAT treats it as stale
status: To Do
assignee: []
created_date: '2026-11-19T01:20:00Z'
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
- [ ] #1 Red host test on the real otFrameIsFreshOutsideTemp()/stamp path: marceld's T+A sequence leaves MsgID 27 unstamped on the current code
- [ ] #2 Fix: the PIC's A answer with the gateway's own outside value stamps MsgID 27 (SAT source O); the OT-Direct cache answer and replayed/parity/PS=1 frames stay excluded; test green
- [ ] #3 Bench or replay: a PIC unit fed with OT= keeps SAT on source O (osrc=O) with no hand-back
<!-- AC:END -->
