---
id: TASK-1315
title: >-
  SAT queues PM=3, PM=15 and PM=48 back to back but the PIC keeps only one
  pending priority message
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T09:37:00Z'
updated_date: '2026-10-25T12:53:00Z'
labels:
  - pic
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 438000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Side finding of the TASK-1314 research (2026-10-23): src/OTGW-firmware/SATcontrol.ino ~3765-3767 queues PM=3, PM=15 and PM=48 in a row. In the PIC gateway firmware 6.x a single prioritymsgid is stored and each PM= overwrites the pending one until a matching response arrives (other-projects/otgw-6.6 gateway.asm SetPrioMessage ~4983-4996, cleared at ~2430-2434); the priority message only goes out in a slot the thermostat gives up. Unless the ESP queue spaces these commands past a served slot, only the last PM= is likely to take effect. Check the ESP queue pacing and the bus (capture), then either space them (send the next PM= after the previous ID's response) or drop the ones that are not needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench or capture evidence of which of MsgID 3/15/48 are actually requested after the three PM= commands (old)
- [ ] #2 All needed IDs are requested (spacing on response) with evidence, or the unneeded PM= removed with the reason recorded
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: gruijter's 1 h capture (20261023-094059.zip, alpha.461, SAT active) contains no PM= command at all, so it cannot show the effect; needs a PIC board on the bench (Classic-S3 not connected) or a capture from an enable/refresh moment that issues the PM= burst. Skipped for now.

2026-10-23 data point (TASK-1314 investigation): on a real PIC 6.8 (1.x unit, port 25238) three PR= reads sent back to back in one TCP write were all answered (3/3 runs). So the PIC does not drop back-to-back PR= commands; whether PM= behaves differently (it queues a request for a later OT slot) is still open.

2026-10-24 research (read-only agent, key claims re-checked by the main session): the real defect is ESP-side. addCommandToQueue (OTGW-Core.ino:3747-3757) with forceQueue=false overwrites any queued entry with the same 2-letter prefix; initSAT queues PM=3, PM=15, PM=48 back to back with forceQueue=false (SATcontrol.ino:3871-3873, verified), so only PM=48 ever reaches the PIC and MsgID 3 (satDetectManufacturer -> quirks MI_500/MIN_MOD_10/NO_REL_MOD, OTGW-Core.ino:3149) is never requested by the gateway. checkCommandResponse also removes on any 'PM:' echo by prefix only. PIC 6.x: single prioritymsgid, overwritten by each PM= (gateway.asm SetPrioMessage ~4984-4996, cleared on the matching response ~2430-2434). otmonitor capslog-1.0.tm sends one PM= and waits for the response id, re-issuing on loss. thermo-nova boot() sends PM=3/15/48 in a row (coordinator/mqtt/opentherm.py:166-172, verified) through HA MQTT. OT-Direct (OTDirect.ino:3369-3392) has no single-slot limit and bypasses the PIC queue. Plan (option B, aligns with thermo-nova's intent that all three are requested, transport fix, not a SAT control change): queue PM=3 only at boot; on the MsgID 3 response queue PM=15, on 15 queue PM=48, each with a timeout advance; queue them with forceQueue=true or a PM-specific dedup and make checkCommandResponse match the PM value. Evidence: host test calling the real addCommandToQueue/handleCommandQueue/checkCommandResponse with a fake sendPICSerial (old: one line PM=48; new: PM=3 then 15 then 48 in order); bus evidence needs a PIC board (Classic-S3 availability to check). Waiting for TASK-1337 (also edits SATcontrol.ino) before implementing.

2026-10-24 implementation (agent + review round): ESP-side fix. PIC boards: pmChainStart() queues PM=3; pmChainOnBoilerResponse(id) (hook in processOT for boiler frames, excluding skipthis/bLocalAnswer/bReplayed) only records the id; pmChainTick() at the top of handleCommandQueue (loop task, outside OTStateLock) advances to PM=15 then PM=48, 60 s timeout per step, then one re-ask of ids that got no answer; each step removes only the chain's own previous PM entry (user PM= from REST/MQTT survive); checkCommandResponse matches the PM value. OT-Direct: the three PM= go directly (no single-slot limit), chain not armed. Review round 1 (opus) caught a CRITICAL deadlock in the first version on OT-Direct (enqueue from inside processOT under the non-recursive OTStateLock -> handleOTDirectCommand -> synthesizeResponse -> processOT again); that build was never flashed. Evidence: tests/test_pic_pm_chain.py (real queue + chain code): HEAD 'FAILED (failures=1, skipped=8)' with 'PM=48;' != 'PM=3;PM=15;PM=48;' (the genuine red), fix 9/9 OK (order on responses, wrong echo, silent boiler 0/60/120 s + one re-ask, only missed ids re-asked, user PM survives, not armed on OT-Direct, hook only records); test_sat_pwm_override 63 OK; test_sat_cmd_cache 4 OK; evaluate 0 fail; build.bat combo SUCCESS; bench OTGW32 (OT-Direct) app OTA alpha.484 + sat_boiler_emulator injecting 180 MsgID 3 boiler frames (B40030004) in 3 min: no crash, uptime 3 min, no crashlog. OPEN: AC1/AC2 need a PIC board (capture of the PM: echoes and the gateway's MsgID 3/15/48 requests, old vs new); no 2.0.0 PIC board on the bench now (Classic-S3 .88.63 stale).

2026-10-25 gruijter capture: not testable at runtime enable; pmChainStart() has one caller, initSAT() boot branch (SATcontrol.ino:4057, initSAT only from setup()). Needs a capture across a restart with SAT enabled (>= 5 min). Runtime enable gap is TASK-1384. Asked gruijter (msg 1563898245145428030).
<!-- SECTION:NOTES:END -->
