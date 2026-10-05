---
id: TASK-1213
title: >-
  Loop-task stalls of 4-6 s and heap dips to 532 B after the OT ID 0 discovery
  publish (TASK-1036 soak)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-04 21:12'
updated_date: '2026-10-05 05:18'
labels:
  - esp32
  - mqtt
  - heap
  - reliability
dependencies: []
priority: high
ordinal: 333000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1036 AC#4 re-soak (2026-10-04, OTGW32, esp32-combo alpha.412+8ad028c, heap_soak_driver 10.25 h, hourly discovery republish, onboard OT replay).
- The loop task stalled for up to 6125 ms.
- The heap reached a since-boot minimum free of 532 B, with a minimum largest block of 7668 B.
- One discovery republish POST and two device/info snapshots got no response.
- Telnet shows the largest stalls (5724 ms at 17:55:13, 4361 ms at 18:55:34) start right after '[drip] OT ID 0 published OK'. The loop is silent for 4-6 s while async_tcp keeps serving REST.
- OT ID 0 is the heaviest discovery set (climate, SAT switches and select, full device block).
Hypothesis (unproven): the drip queues ID 0's roughly 20 large configs into the MQTT client at once, and the loop then blocks while the client writes that backlog to a slow broker; the heap dip may be the same burst. Confounds to separate: the broker ran in Docker/WSL on a host at 98% memory, and a passive telnet logger was attached for part of the run.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced on the bench: a discovery republish (or the ID 0 drip step alone) shows the loop stall and the heap dip, with the culprit section named by telnet or a diag counter, and with the broker on a host that is not memory-starved
- [ ] #2 Root cause identified (which call blocks the loop, and what allocates the dip)
- [ ] #3 Fixed with old-vs-fix evidence: on the same procedure, max loop gap under 1 s and min largest block at or above 8192 B; build green for the three targets; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Maintainer choice 2026-10-05: a write budget, within ADR-131 (no internal MQTT task).
A true per-loop() byte budget is impossible without changing espMqttClient: _checkOutbox() writes the whole outbox and ClientSync::write() blocks. A library change needs maintainer approval and is not planned. So the budget sits on the producer side:
1. Drip gate: loopMQTTDiscovery() publishes the next OT ID only when MQTTclient.queueSize() == 0 (the outbox has drained), so queued configs never pile up in heap across IDs.
2. Split the heavy ID 0 set over drip ticks with a resumable sub-index (sensors/binary sensors, then climate, then SAT switches in groups, then the select). Each tick queues at most about one TCP send buffer of configs (target 4 KB or less), so the blocking write in loop() stays short.
3. Keep the all-or-nothing pending rule (TASK-1206): the ID 0 pending bit clears only after the last sub-step published.
Verify: host harness (the dispatcher still publishes the same set of payloads in the same order across ticks); bench old vs fix with 'tc netem delay 500ms' on the rig broker during a discovery republish. Target: max loop gap under 1 s, min largest block at or above 8192, no lost configs (retained count unchanged). Builds wait until host memory allows (maintainer stops llama-server).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Investigation 2026-10-04 (bench OTGW32 alpha.412, host memory about 81%, broker in Docker/WSL on the laptop):
1) Healthy-host repro: a discovery republish drained in 267 s. Max loop stall 344 ms, right after '[drip] OT ID 0 published OK'; min largest block 24564. In the soak, with the host at 98% memory, the same step stalled 4-6 s. So the stall scales with broker/network latency.
2) Mechanism, read in code and not yet proven:
 - MQTTclient is espMqttClient with UseInternalTask::NO (MQTTstuff.ino:209), pumped only by MQTTclient.loop() on the loop task (:1103).
 - espMqttClient::_checkOutbox() writes the whole outbox in one call, while _sendPacket() > 0.
 - ClientSync::write() is NetworkClient::write(), a blocking socket write: up to WIFI_CLIENT_MAX_WRITE_RETRY attempts, each with a select() timeout and SO_SNDTIMEO = _timeout (framework NetworkClient.cpp:388-418).
 - ID 0 queues about 20 configs of about 900 B (roughly 18 KB). More than the TCP send buffer, so the loop waits until the broker drains it.
3) Broker-pause test (docker pause mosquitto for 3 s on the ID 0 drip step): max stall 1276 ms (vs 344 ms without the pause), and the top stall sat right after a drip publish. Inconclusive as proof: docker pause freezes only the broker process while the host kernel keeps ACKing into its receive buffer, so this is not a faithful slow broker.
Next:
- A faithful slow-broker test, either tc/netem delay on the broker's interface or a throttling TCP proxy between device and broker, run old vs a fix.
- Fix options for the maintainer:
  (a) a per-loop write budget, i.e. cap the bytes or time MQTTclient.loop() spends per call;
  (b) UseInternalTask::YES so the writes block an MQTT task instead of the loop (an ADR-131 concern);
  (c) a short send timeout on the socket with drop-and-reconnect on stall, as the 1.x line does.
Evidence: scratchpad t1213_repro1.txt, t1213_pause3s.txt.

4) Faithful slow-broker test, 2026-10-04 23:4x: a helper container in mosquitto's network namespace added 'tc qdisc netem delay 500ms' on eth0 (removed afterwards; qdisc back to noqueue).
Result of a discovery republish:
- 23 loop stalls over 200 ms, together 6.4 s; max 967 ms. The largest followed a drip publish ('OT ID 5 published OK'); ID 0 gave 306 ms.
- The heap reached hd_min_free_heap 760 B and min largest block 14836 B (this boot started at the 23:2x filesystem flash, so the dip happened in this test series), with REST BUSY 503s at maxblock 9716.
- MQTT stayed connected; the drain took 269 s.
Reading:
- Broker latency drives both symptoms. The stalls come from blocking socket writes in MQTTclient.loop() on the loop task.
- The heap dip most likely comes from the outbox holding the queued discovery packets in heap until the slow broker takes them. This part is an inference: no per-allocation trace yet.
- 500 ms RTT does not reach the soak's 4-6 s stalls; a worse broker (WSL under memory pressure) does.
Root cause: identified at mechanism level and reproduced as a latency dependency. Not yet proven with an old-vs-fix pair, which needs a fix.
Fix design is a maintainer decision (it touches ADR-131's MQTT engine model).

Implementation 2026-10-05 (working tree, not committed; build blocked on host memory):
- MQTTstuff.ino:
  - g_mqttPublishedBytes counts the bytes handed to the client in mqttPublishRaw.
  - A DiscoveryBudget plus discBudgetTake() sits in front of every config unit in doAutoConfigureMsgid() and inside the SAT zone and PV-boost composers, one unit per entity.
  - The drip passes a 4096 B budget, resumes an unfinished ID at the saved index (sDripResumeId/Skip), and keeps the pending bit until the ID completes.
  - The drip publishes only when MQTTclient.queueSize() == 0.
  - The ID 244 button + selects stay one atomic set (TASK-1206).
  - The device block rides every tick of an ID exactly as before, so the payloads are unchanged.
  - Direct callers (no budget) behave as before.
- Host harness part F (test/host/test_ha_discovery_json.py --old-rev 8795bacc0) gives RESULT: PASS:
  - Per permutation and ID, the budgeted drip publishes exactly the dispatcher's configs in the same order.
  - Every tick stays within budget + two configs: the largest tick is 5112 B, bound 6242.
  - OT ID 0 is split over 6 ticks.
  - Unbudgeted, ID 0 went out as 22714 B in one call (modern_pic).
- evaluate.py --quick 71/0.
Pending: build, then bench old vs fix under 'tc netem delay 500ms' (max loop gap, hd_min_free_heap, retained config count unchanged).
<!-- SECTION:NOTES:END -->
