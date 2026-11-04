---
id: TASK-1315
title: >-
  SAT queues PM=3, PM=15 and PM=48 back to back but the PIC keeps only one
  pending priority message
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T09:37:00Z'
updated_date: '2026-11-02T01:02:00Z'
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
- [x] #2 All needed IDs are requested (spacing on response) with evidence, or the unneeded PM= removed with the reason recorded
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: gruijter's 1 h capture (20261023-094059.zip, alpha.461, SAT active) contains no PM= command at all, so it cannot show the effect; needs a PIC board on the bench (Classic-S3 not connected) or a capture from an enable/refresh moment that issues the PM= burst. Skipped for now.

2026-10-23 data point (TASK-1314 investigation): on a real PIC 6.8 (1.x unit, port 25238) three PR= reads sent back to back in one TCP write were all answered (3/3 runs). So the PIC does not drop back-to-back PR= commands; whether PM= behaves differently (it queues a request for a later OT slot) is still open.

2026-10-24 research (read-only agent, key claims re-checked by the main session): the real defect is ESP-side. addCommandToQueue (OTGW-Core.ino:3747-3757) with forceQueue=false overwrites any queued entry with the same 2-letter prefix; initSAT queues PM=3, PM=15, PM=48 back to back with forceQueue=false (SATcontrol.ino:3871-3873, verified), so only PM=48 ever reaches the PIC and MsgID 3 (satDetectManufacturer -> quirks MI_500/MIN_MOD_10/NO_REL_MOD, OTGW-Core.ino:3149) is never requested by the gateway. checkCommandResponse also removes on any 'PM:' echo by prefix only. PIC 6.x: single prioritymsgid, overwritten by each PM= (gateway.asm SetPrioMessage ~4984-4996, cleared on the matching response ~2430-2434). otmonitor capslog-1.0.tm sends one PM= and waits for the response id, re-issuing on loss. thermo-nova boot() sends PM=3/15/48 in a row (coordinator/mqtt/opentherm.py:166-172, verified) through HA MQTT. OT-Direct (OTDirect.ino:3369-3392) has no single-slot limit and bypasses the PIC queue. Plan (option B, aligns with thermo-nova's intent that all three are requested, transport fix, not a SAT control change): queue PM=3 only at boot; on the MsgID 3 response queue PM=15, on 15 queue PM=48, each with a timeout advance; queue them with forceQueue=true or a PM-specific dedup and make checkCommandResponse match the PM value. Evidence: host test calling the real addCommandToQueue/handleCommandQueue/checkCommandResponse with a fake sendPICSerial (old: one line PM=48; new: PM=3 then 15 then 48 in order); bus evidence needs a PIC board (Classic-S3 availability to check). Waiting for TASK-1337 (also edits SATcontrol.ino) before implementing.

2026-10-24 implementation (agent + review round): ESP-side fix. PIC boards: pmChainStart() queues PM=3; pmChainOnBoilerResponse(id) (hook in processOT for boiler frames, excluding skipthis/bLocalAnswer/bReplayed) only records the id; pmChainTick() at the top of handleCommandQueue (loop task, outside OTStateLock) advances to PM=15 then PM=48, 60 s timeout per step, then one re-ask of ids that got no answer; each step removes only the chain's own previous PM entry (user PM= from REST/MQTT survive); checkCommandResponse matches the PM value. OT-Direct: the three PM= go directly (no single-slot limit), chain not armed. Review round 1 (opus) caught a CRITICAL deadlock in the first version on OT-Direct (enqueue from inside processOT under the non-recursive OTStateLock -> handleOTDirectCommand -> synthesizeResponse -> processOT again); that build was never flashed. Evidence: tests/test_pic_pm_chain.py (real queue + chain code): HEAD 'FAILED (failures=1, skipped=8)' with 'PM=48;' != 'PM=3;PM=15;PM=48;' (the genuine red), fix 9/9 OK (order on responses, wrong echo, silent boiler 0/60/120 s + one re-ask, only missed ids re-asked, user PM survives, not armed on OT-Direct, hook only records); test_sat_pwm_override 63 OK; test_sat_cmd_cache 4 OK; evaluate 0 fail; build.bat combo SUCCESS; bench OTGW32 (OT-Direct) app OTA alpha.484 + sat_boiler_emulator injecting 180 MsgID 3 boiler frames (B40030004) in 3 min: no crash, uptime 3 min, no crashlog. OPEN: AC1/AC2 need a PIC board (capture of the PM: echoes and the gateway's MsgID 3/15/48 requests, old vs new); no 2.0.0 PIC board on the bench now (Classic-S3 .88.63 stale).

2026-10-25 gruijter capture: not testable at runtime enable; pmChainStart() has one caller, initSAT() boot branch (SATcontrol.ino:4057, initSAT only from setup()). Needs a capture across a restart with SAT enabled (>= 5 min). Runtime enable gap is TASK-1384. Asked gruijter (msg 1563898245145428030).

2026-11-01 bench evidence (Robert asked to close it): Classic+S3 .63, PIC 6.8 gateway, firmware 2.0.0-beta.1+2ec9ce9 (contains e344fa1f1), SAT enabled, SAT simulation switched off for the run (it blocks PIC TX), no boiler and no thermostat on the bus. Reboot 21:03:41, telnet from 21:03:55 (the boot PM=3 itself fell before the telnet connect). Telnet log: 21:04:49 'Sending to Serial [PM=15]' -> PIC 'PM: 15'; 21:05:49 [PM=48] -> 'PM: 48'; retry round of the unanswered ids: 21:06:49 [PM=3] -> 'PM: 3', 21:07:50 [PM=15] -> 'PM: 15', 21:08:50 [PM=48] -> 'PM: 48'; then no PM= up to 21:10:34 (chain stops after one retry). So each PM= reaches the PIC on its own, 60 s apart (timeout per step without a boiler answer), none overwritten in the queue; the PIC accepts each. AC#2 checked on that ESP->PIC evidence. Not shown: the boiler answering MsgID 3/15/48 (no boiler on this bench) and the old firmware on the bench (AC#1 'old' evidence rests on the 2026-10-24 code reading of addCommandToQueue's same-prefix overwrite, not a bench run). Robert asked to close the task on this test; AC#1 left unchecked as waived. Log kept locally (session tmp, telnet-run2-pmchain.log).

2026-11-01 reopened by Robert: show that when three PM= commands arrive shortly after each other, all three really get through, not only the last.

2026-11-01 fix (Robert: PM= unique in the queue, and TP= the same way): addCommandToQueue treats PM= and TP= as unique per value (exact repeat refreshes; TP= no longer append-only). tests/test_pic_pm_chain.py new cases user_three (3x PM= from REST/MQTT) and user_tp; old OTGW-Core.ino FAILED 2 ('1' != '3' queued; TP repeat '4' != '3'), fix 11/11 OK; queue-related tests (otd_ch_override, sat_cmd_cache, dhw_sw_sync, max_tset, no_return_ignition, ot_loss, push_setpoint, restart_quiet, pwm_override, hp_pwm) OK; evaluate --quick 80/0. Version 2.0.0-beta.3. Bench (.63 PIC 6.8): three PM= via REST in quick succession, expect three PIC echoes; boiler answers cannot be shown (no boiler on the bench).

2026-11-01 bench .63 on 2.0.0-beta.4+2905bd1 (SAT simulation off): three PM= via REST POST /api/v2/otgw/commands at 23:34:52 (HTTP 202 each, 26-65 ms) -> queue slots [0] PM=3, [1] PM=15, [2] PM=48 (no overwrite); 23:34:54.68 sent to the PIC PM=3, PM=15, PM=48 within 4 ms and echoed 'PM: 3', 'PM: 15', 'PM: 48'. Follow-up as 2.0.0-beta.5 (Robert: one second apart): handleCommandQueue sends at most one PM=/TP= per PRIO_MSG_SPACING_MS 1000 ms; tests/test_pic_pm_chain.py user_three now checks send times: beta.4 code FAILED ('sent less than 1 s apart: [0, 0, 0]'), fix 11/11; queue tests and evaluate 80/0 green.

2026-11-02 bench .63 on 2.0.0-beta.5+d0efe73 (SAT simulation off): three PM= via REST POST /api/v2/otgw/commands (202 each) queued in slots [0] PM=3, [1] PM=15, [2] PM=48 and sent to the PIC at 02:02:01.745, 02:02:02.747, 02:02:03.747 (1.0 s apart), each echoed 'PM: 3', 'PM: 15', 'PM: 48'. Posted (Discord msg 1566618474996961435).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PM= and TP= are unique per value in the PIC command queue (three different PM= in a row are three entries; an exact repeat refreshes its entry; TP= no longer append-only), and the queue sends at most one PM=/TP= per second, since both load the PIC's single priority-message slot (otgw-6.6 gateway.asm SetPrioMessage/SlaveParameter). SAT's boot chain (e344fa1f1) still waits for the boiler answer per id. Shipped as 2.0.0-beta.3 (efaa0a128) and beta.5 (0ab3edd99). Evidence: tests/test_pic_pm_chain.py red on old code (3 PM= -> 1 entry; TP repeat queued twice; sends at [0,0,0] ms), green 11/11 on the fix; bench .63 PIC 6.8: three PM= via REST reach the PIC 1.0 s apart, each echoed. Not verified: boiler answers to MsgID 3/15/48 (no boiler on the bench); AC#1 'old' bench evidence waived by Robert (old behaviour shown by the host test).
<!-- SECTION:FINAL_SUMMARY:END -->
