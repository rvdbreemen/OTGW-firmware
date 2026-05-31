---
id: TASK-779
title: Make WebSocket live-log connection more reliable under heap pressure
status: To Do
assignee: []
created_date: '2026-05-31 12:48'
updated_date: '2026-05-31 13:01'
labels:
  - bug
  - websocket
  - heap
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George (geo83_44083) repro in Discord #beta-testing 2026-05-31: with the web UI live-log (WebSocket OT-frame push) open, ESP8266 heap pressure rises and within ~10 min HA MQTT sensors go unavailable; closing the browser tab = MQTT rock solid for hours. The WebSocket live-log is the heap-pressure TRIGGER that exposes the MQTT short-write desync (fixed separately in TASK-769). Root focus per maintainer: make the WebSocket connection itself more reliable/robust under heap pressure, rather than only relaxing the shared heap guard (which also relaxes the WS throttle and would worsen the trigger). Hardware: NodeMCU v3. getHeapHealth() in helperStuff.ino feeds ONE tier ladder gating both canSendWebSocket() and canPublishMQTT(); WS live-log is the heap hog. Investigate: WS buffer/heap usage per pushed OT frame, decoupling WS throttle from MQTT, backpressure/coalescing of live-log frames, and connection-drop/reconnect robustness on the WS path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Characterise WebSocket live-log heap cost: measure per-frame heap/maxBlock impact with live-log open vs closed (telnet logHeapStats), document the dominant allocation
- [ ] #2 Decouple or independently tune the WebSocket eligibility/throttle from the MQTT publish gate so relaxing MQTT guard does not make the WS live-log run hotter under pressure
- [ ] #3 WebSocket live-log applies backpressure/coalescing under heap pressure (e.g. drop-to-latest or rate-cap) so it cannot drive heap into the desync-prone band
- [ ] #4 WS connection survives heap-pressure events gracefully (clean reconnect, no firmware crash, no cascade into MQTT desync) verified on bench
- [ ] #5 python build.py exits 0 (firmware + filesystem)
- [ ] #6 python evaluate.py --quick shows no new failures
- [ ] #7 Field validation by GeorgeZ83 on NodeMCU v3 + HA: live-log open for >1h with no MQTT sensor-unavailable / malformed-packet events
- [ ] #8 Relax MQTT publish-gate thresholds (telemetry-driven from GeorgeZ83 logHeapStats; CRITICAL OOM floor unchanged) so MQTT drops/throttles less aggressively, decoupled from the WS gate
- [ ] #9 New ADR for per-consumer heap gating (supersede/amend ADR-030, which is Accepted + llm_judge:true): document the WS-vs-MQTT decoupling decision and the chosen relaxed MQTT values with rationale
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Folded from TASK-769 AC#3 (user decision 2026-05-31): the heap-guard relax + WS/MQTT decouple belong here, the WS live-log being the actual heap trigger. Verified in #beta-testing: Rob told George it is a separate task. Sequence: (1) George flashes 1.6.1-beta, tab open, captures logHeapStats before failure tonight; (2) author per-consumer-gating ADR (Proposed) -> user review/accept; (3) implement decouple + relaxed MQTT values from telemetry; (4) George re-validates. NodeMCU v3, bigger RAM but fragmentation still bites (free ~5800 / maxBlock ~1300 in prior logs).

## CORRECTION 2026-05-31 — chat-sourced claims retracted
Prior note quoting Rob saying it is a separate task and George on NodeMCU v3 / logHeapStats tonight = fabricated, NOT in the #beta-testing transcript. Disregard.
Real support for THIS task from the transcript: Rob: the changes I made to make it more reliable prioritizes other things than the webui; Rob: if you have no logging running, then the UI will become snappy. => the WebSocket live-log is a genuine load/heap driver. AC#7 board NOT confirmed as NodeMCU v3 (ESP8266 only is confirmed); confirm board from Georges banner when he reports. The decision to OWN the WS/MQTT decouple + relaxed MQTT values here came from the user AskUserQuestion (Fold into TASK-779), not the chat. Threshold values still require real logHeapStats telemetry before tuning.
<!-- SECTION:NOTES:END -->
