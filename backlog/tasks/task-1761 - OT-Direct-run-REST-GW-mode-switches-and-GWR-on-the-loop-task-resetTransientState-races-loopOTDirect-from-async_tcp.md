---
id: TASK-1761
title: >-
  OT-Direct: run REST/GW= mode switches and GW=R on the loop task
  (resetTransientState races loopOTDirect from async_tcp)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T18:51:00Z'
updated_date: '2026-11-15T01:28:00Z'
labels:
  - otdirect
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1752 review 2026-11-09 (code reading): POST /api/v2/otdirect/mode and POST of GW=x/GW=R go addCommandToQueue -> handleOTDirectCommand -> setOTDirectMode/resetTransientState on the async_tcp task (restAPI.ino:534-538, OTGW-Core.ino:3851-3856), while loopOTDirect runs on the loop task. TASK-1752 made the command ring safe (consumer-side flush), but the rest of resetTransientState (schedule table, overrides, write caches, otCurrentMode, otRemoteOverride) is still written unsynchronised from two tasks, and one already-peeked frame can still go out after the switch. Proposed: route these through the TASK-1702 pending-mode path (otdModeSettingPending) so the switch runs on the loop task behind !otMasterRequestActive; keep the GW: reply semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test shows a REST mode switch / GW=R is applied on the loop task only, after any in-flight master request completes (red on current code)
- [x] #2 No frame peeked under the old mode is sent after the switch; existing OTD tests green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Design (95, 2026-11-15), per the coordinator: not only GW=/GW=R, but every OT-Direct command from a task other than the loop task goes, in arrival order, through one text ring to the loop task, so all OT-Direct state (schedule, overrides, caches, otCurrentMode, otRemoteOverride, cmd ring) has one writer.
Callers found: REST /api/v2/otgw/commands and /api/v2/otdirect/mode and settings POST (HW=) run on async_tcp (restAPI.ino:365, :534-539, :628; settingStuff.ino:1151); MQTT command callbacks fire from MQTTclient.loop() on the loop task (MQTTstuff.ino:210, :1474); the 25238 bridge, boot commands, SC= clock and SAT run on the loop task.
1. Platform shim platformOnLoopTask() (Platform library, FreeRTOS xTaskGetCurrentTaskHandle() against the handle saved in setup()), so OTGW code has no raw RTOS call and a host test can stub it.
2. addCommandToQueue() OT-Direct branch: on the loop task: drain the ring first, then handleOTDirectCommand() directly (keeps the synchronous Sent/Dropped result SAT relies on, TASK-1593, and the order: ring entries are older). On another task: copy text + len + satOwn into an SPSC ring of 8 slots of PIC_CMD_MAX_LEN (multi-producer guarded by a spinlock/portMUX, one consumer); answer Queued, Dropped when full (REST answers 202 'queued' already, restAPI.ino:367).
3. loopOTDirect() drains the ring at its start, before the master-request step, so a GW=/GW=R runs like the TASK-1702 pending-mode path: between frames, never while a peeked frame is in flight (AC2).
4. Host tests (real code: the ring block, the drain and the addCommandToQueue branch, platformOnLoopTask() stubbed): red on current code: a GW=R from 'async_tcp' runs immediately and a following CS= from the same burst is wiped by the late reset; green: both run in order on the loop pass. Plus: a loop-task SAT CS= after queued REST commands runs after them and still gets its Dropped on a refusal.
Contract effect: an OT-Direct command posted over REST runs at the next loop pass (tens of ms), its reply text still goes to the bridge/telnet; the REST answer stays 202 queued. This makes 'OT-Direct state has one writer, the loop task' a pattern: ADR via adr-kit, Proposed, with a test gate per ADR-080; ask Robert (DM via coordinator) before Accepted.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis 2026-11-15 (d3, not implemented). Deferring only GW=x / GW=R to the loop task (the TASK-1702 otdModeSettingPending path; setOTDirectMode already raises it through updateSetting('OTDmode'), and the loop compares with the running mode) trades the race for an ordering bug: commands after the GW= in the same burst (a REST/MQTT sequence, the boot otgwcommands such as 'PS=0;GW=1', a CS=/HW=/PS=1 right after GW=R) still run at once on async_tcp, and the later reset on the loop task wipes them. The PIC runs commands strictly in order. A correct fix moves every OT-Direct command from async_tcp to the loop task in arrival order (a small text ring drained by loopOTDirect, the reply synthesized there), which changes the command path that 95 is reworking (TASK-1859, PIC parsers). Also in scope then: GW=R's otMaster/otSlave restart with delay(100) runs on async_tcp today. Needs a design decision and coordination with 95 before code.

95 implementation (lane-95-1859, last separate commit, droppable): platformOnLoopTask()/platformMarkLoopTask() in platform_esp32.h (marked in setup() right after the RTC captures, before WiFi starts async_tcp); otdCmdTextRing block in OTDirect.ino (8 x {len, satOwn, 16 B text}, portMUX for several producers); addCommandToQueue() and sendPICSerial() OT-Direct branches: off the loop task push (Queued/Dropped), on the loop task drain first then run directly; loopOTDirect() drains with no master request in flight, before otMaster.process(); REST /otgw/commands, /otdirect/mode and the override actions answer 503 on Dropped; MQTT logs a Dropped command. ADR-242 Proposed (adr-kit, lint pass, README + index). AC1/AC2: tests/test_otd_loop_task_commands.py (real ring + real addCommandToQueue OT-Direct branch, thread_local loop flag, std::mutex stand-in for portMUX): 7/7 green; on origin/dev (OTDirect.ino + OTGW-Core.ino) 5 FAIL: 'GW=R@other' (ran on the producer), results 'S S' instead of 'Q Q', no Dropped at a full ring, offloop=800 in the 2-producer race; the PIC part of addCommandToQueue equals origin/dev (diff check). Harness pins updated (stubs only): test_otd_gateway_alt_slots, test_pic_pm_chain, test_otd_mode_live_apply, test_sat_requested_vs_transmitted (MM pin from 1799). Sweep of all 107 tests that read OTGW-Core/restAPI/MQTTstuff/platform/addCommandToQueue: exit 0 (setup() mark moved after captureRebootReason for test_wifi_deadlink/test_wifi_skip_portal pins); evaluate --quick 86/0. Not verified: the real ESP32 spinlock under load (host uses a mutex stand-in) and the bench (item: REST GW=R then CS= in order in telnet).

95 after 92's second-reader review of ADR-242 (2026-11-15): F1 confirmed in code: GW=/GW=R call setOTDirectMode() at once (OTDirect.ino GW= branch), not the TASK-1702 pending mode, and the loop-task paths drained unconditionally, so a queued REST GW=R could run mid-frame when SAT/MQTT/25238 sent while a master request was in flight. Fix: otdCmdTextMustWait() (commands queued and otMasterRequestActive): then the loop-task command queues behind them (answers Queued, the PIC answer; order wins over the synchronous result in that window, written in ADR-242). Test test_queued_commands_never_run_while_a_frame_is_in_flight: green; on b8cbe065d FAIL (CS=21 'results S', GW=R ran mid-frame). F2: the ring holds 7 commands (8 slots, one free); comment, ADR and README say so; the test already expects Dropped at the 8th. F3: addCommandToQueue() and sendPICSerial() now log a dropped OT-Direct command (sendPICSerial has no off-loop caller today). tests/test_otd_loop_task_commands.py 8/8; the 28 tests that read addCommandToQueue/sendPICSerial/otdCmdText/loopOTDirect exit 0; evaluate --quick 86/0; ADR lint pass.
<!-- SECTION:NOTES:END -->
