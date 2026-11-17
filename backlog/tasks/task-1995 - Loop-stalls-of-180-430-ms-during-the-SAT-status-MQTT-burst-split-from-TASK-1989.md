---
id: TASK-1995
title: >-
  Loop stalls of 180-430 ms during the SAT status MQTT burst (split from
  TASK-1989)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-16T06:28:00Z'
updated_date: '2026-11-17T09:50:00Z'
labels:
  - performance
dependencies: []
priority: low
ordinal: 1019000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Split from TASK-1989 (out of its scope, coordinator 2026-11-16). In gruijter's beta.250 capture 34 of the 92 [loop-stall] lines (200-600 ms) sit in an MQTT burst: satSendStatus plus the sat/* settings topics at :x6:25 / :x6:55, with an internal gap of 180-400 ms between addCommandToQueue and satControlLoopRun inside the window. Not a file write, so the TASK-1989 file-write work does not touch it. Find what runs in that step with the slowest-step instrumentation of TASK-1989 (92c914047) on a capture, then decide fix or keep.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A capture on a build with the slowest-step stall line names the step of the burst stalls
- [ ] #2 Fix with old-vs-fix numbers, or a recorded reason why it stays (bounded, harmless)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Step A (instrumentation only, own commit): time each sub-step of loopLittleFSStatus() (helperStuff.ino:252-270): the 5-min write probe updateLittleFSStatus('/.health') (= platformFSInfo() traversal + open('w') + println + flush + close), the 10-s refreshLittleFSFileFlags(), loopDallasLabels(), loopSatMarkers() and the 60-s mount check platformFSInfo(). Print one telnet line '[fs-slow] <step> <ms> ms' when a sub-step takes >= 100 ms (one millis() pair per step, no output otherwise), plus a CPROBE_FS_END probe after loopLittleFSStatus() in doBackgroundTasks() so the [loop-stall] line can split step 5->8 into 'before the FS check' (loopWifi, telnet re-listen, Ethernet, heap check) and 'FS check -> handleMQTT' (handleDebug, handleOTGWstream). Evidence: a 60-min telnet capture by 09 on .61 and .63 naming the step of each stall. Step B (behaviour change, only after Step A names the culprit and after the coordinator's OK): if it is the write probe and/or platformFSInfo(), run them as a job on the TASK-1989 fsWriter task (core 0) instead of the loop: a new FSW_OP_PROBE that does the traversal + the /.health write and publishes the result to LittleFSmounted (a bool, one writer); the loop keeps reading LittleFSmounted. Alternatives weighed: (1) drop the write probe (it is the only write-health signal; refused), (2) a longer interval (fewer stalls, same size; weaker), (3) fsWriter job (removes it from the loop; chosen if Step A confirms). Risk of (3): TASK-1865 moved the traversal OFF async_tcp because a hang there starved a task on the same core; the fsWriter also runs on the protocol core, but at priority 1 and it already does the SAT state writes there; a watchdog on the job duration (log > 1 s) is part of the plan. Tests: host test on the real loopLittleFSStatus() scheduling (probe job queued, not run on the caller) red on the old code; bench: 60 min with 0 stalls in the FS step.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-17 d3 (analysis + instrumentation, no behaviour change). The slowest-step stall line already exists on origin: OTGW-firmware.ino:1185-1190 '[loop-stall] %lu ms gap, slowest step %u->%u %lu ms' (TASK-1989, 92c914047), with probes at loop/doBackgroundTasks/handleMQTT/satControlLoop granularity (MQTTstuff.h CPROBE_* enum). Gap: the whole of satControlLoopRun() was one step (CPROBE_DISCOVERY or the previous probe -> CPROBE_SAT_RUN). Added (local, goes into d3's next build): CPROBE_SAT_PUB_BEGIN / CPROBE_SAT_PUB_END (MQTTstuff.h, appended, numbered 18/19) around satPublishMQTT() (SATcontrol.ino, after its gates; END from a scope guard on every return), only under #ifdef OTGW_CONN_PROBE. All six satPublishMQTT() callers sit in satControlLoopRun() (loop task). Read of the burst path, per step, what can block: (1) publishIfChanged*() (SATmqttPublish.cpp:64-81): compare, satOutboxFull() gate (depth vs SAT_MQTT_OUTBOX_LIMIT, connect pace hold), dtostrf into 16 B: no blocking. (2) sendMQTTData() (MQTTstuff.ino:1979-2014): snprintf_P topic, MQTTDebugTf (a telnet write only with the MQTT debug flag on; a slow telnet client could then block), diagRec append (only when recording), mqttPublishRaw(). (3) mqttPublishRaw() (MQTTstuff.ino:344-348): MQTTclient.publish() = espMqttClient 1.7.2 vendored, UseInternalTask::NO (MQTTstuff.ino:217): EMC_SEMAPHORE_TAKE (xSemaphoreTake portMAX_DELAY, Helpers.h:16) + _addPacket (heap alloc + memcpy into the Outbox), no socket I/O; the semaphore can only wait if another task publishes at the same time (not checked which tasks do). (4) No flash write and no String in this path. (5) The socket writes happen later in MQTTclient.loop() -> _sendPacket() (MqttClient.cpp:249-250) through the sync transport, which can block on a full TCP send window: that is step CPROBE_MQTT_PRE_LOOP -> CPROBE_MQTT_POST_LOOP, not the SAT step. Hypothesis to test with the capture (not verified): the burst stall lands in the MQTT loop step (flushing ~50 queued packets) or in loopMQTTDiscovery (step TIMERS -> DISCOVERY), rather than in satPublishMQTT itself. AC1 needs a capture on the build that carries these probes.

2026-11-17 first capture (09, .61 on 01d24c5 = d3's build with the SAT publish probes, telnet 07:45:25-08:25:05, ~40 min): 4 [loop-stall] lines, ALL step 5->8 (CPROBE_APP_END -> CPROBE_PRE_MQTT), 187-581 ms, at :x1:53, :x6:53, :x9:53, :x1:53; 0 with step 18->19 (satPublishMQTT). So the stall is not in the SAT status publisher. Step 5->8 runs (OTGW-firmware.ino:1253 -> :1127): the start of doBackgroundTasks(): loopWifi(), telnetRelistenTick(), loopEthernet(), the heap check, loopLittleFSStatus() (helperStuff.ino:252), handleDebug(), handleOTGWstream(). Prime suspect, not yet proven: loopLittleFSStatus() runs updateLittleFSStatus('/.health') every 5 min (DECLARE_TIMER_MIN(fsWriteProbe, 5), :253-259), which is a LittleFS.open('w') + println write on the loop task: the same 250-410 ms block-allocation cost TASK-1989 measured. The 5-min rhythm fits :x1:53 and :x6:53; the :x9:53 stall does not fit it, so a second cause in the same step (loopWifi?) is possible. Next: a capture with a probe after loopLittleFSStatus() (or the fsWriteProbe tick logged) to split the step; a fix (move the write probe off the loop task, e.g. through the fsWriter) is a behaviour change, to plan separately.

2026-11-17 Step A implemented (lane-1995, src uncommitted until the build bump): fsSlowNote() in helperStuff.ino prints '[fs-slow] <step> <ms> ms' at >= 100 ms for write-probe, file-flags, dallas-labels, sat-markers and mount-check inside loopLittleFSStatus(); CPROBE_FS_END (MQTTstuff.h, 20) right after loopLittleFSStatus() in doBackgroundTasks(), so the [loop-stall] line splits step 5->8 into 5->20 (WiFi/telnet re-listen/Ethernet/heap/FS) and 20->8 (handleDebug, handleOTGWstream). New tests/test_fs_slow_note.py (real fsSlowNote compiled + source checks): 4/4 on the fix, 8 FAIL on the old code (OTGW_HELPER_INO/OTGW_MAIN_INO = HEAD copies). test_health_no_fs_flash_ops got a fsSlowNote/PSTR stub (5/5 on new and old); dallas labels, rest fs flags, sat markers, loop step watch, connect probe green; evaluate 88/0. Next: build in d3's slot, then a 60-min capture by 09 on .61 + .63.

2026-11-17 10:50 STATUS at ALL STOP (coordinator): lane-1995 in D:\Users\Robert\Documents\GitHub\RvdB\wt-1995, HEAD ee34f5f27 on origin 42e26c101, 2 local commits (57ccc74ac-equivalent test commit + Step A diag commit, beta.366), NOT pushed. A build of ee34f5f27 started 10:49:28 just before the stop and is left to finish (killing it risks a stale build lock); its artifacts land in wt-1995\build\ (2.0.0-beta.366+ee34f5f). NEXT (tomorrow): (1) check the build log (scratchpad build-366.log: 'Build completed successfully', fresh artifacts, githash = HEAD); if origin moved, rebase, renumber and rebuild; (2) ask the coordinator for a bench: 09 runs a 60-min telnet capture on .61 + .63 on 366 and collects every [loop-stall] (step 5->20 vs 20->8) and [fs-slow] line; (3) push slot + zip post; (4) Step B (move the /.health probe + traversal onto the fsWriter task) only after the capture names the culprit and with the coordinator's OK.
<!-- SECTION:NOTES:END -->
