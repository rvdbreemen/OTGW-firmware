---
id: TASK-779
title: Make WebSocket live-log connection more reliable under heap pressure
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 12:48'
updated_date: '2026-06-06 05:41'
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

2026-06-02 (investigate + draft-ADR scope, per user decision):
- Read heap-gating path: getHeapHealth() helperStuff.ino:940 feeds BOTH canSendWebSocket() (989) and canPublishMQTT() (1043). Key finding: throttle INTERVALS already split (WEBSOCKET_THROTTLE_MS_* vs MQTT_THROTTLE_MS_*, lines 915-918); only the TIER BOUNDARIES are shared. So the decouple is at threshold-ladder level, not interval level.
- WS allocation (AC#1 partial, static): sendLogToWebSocket -> webSocket.broadcastTXT (webSocketStuff.ino:264-267); broadcastTXT copies payload per client x MAX_WEBSOCKET_CLIENTS=3 -> dominant per-frame heap cost. Producer=live-log, victim=MQTT. Quantitative per-frame numbers still need George logHeapStats (live-log open vs closed) on real device.
- ADR-030 thresholds documented (3072/5120/8192) are STALE vs code (1536/3072/5120 + maxBlock promote). New ADR revisits the single-ladder DECISION, not the numbers.
- Authored docs/adr/ADR-083-per-consumer-heap-gating.md (Status: Proposed): per-consumer threshold ladders + shared CRITICAL OOM floor + WS drop-to-latest coalescing. Strict lint gates pass (completeness/consistency/evidence); clarity advisory only (standard acronyms).

BLOCKED ACs (not self-verifiable here):
- AC#1 quantitative heap measurement: needs field logHeapStats.
- AC#7 field validation by GeorgeZ83 on bench (>1h).
- AC#8 relaxed MQTT threshold VALUES: deferred in ADR-083 pending telemetry (structure decided, numbers not invented).
- AC#9 ADR: drafted Proposed; NOT Accepted (maintainer sign-off required; relaxed-values portion telemetry-gated). Leave unchecked until accepted.

Impl of decouple code (AC#2/#3) intentionally NOT done in this scope (user chose investigate+draft-ADR-only); unblocks once ADR-083 Accepted + telemetry in.

2026-06-05 field report (GeorgeZ83, #beta-testing) — 1.7.0-beta+6b79d62, MQTT DISABLED, 12-min telnet capture (putty-telnet.log):
- Banner heap: free 2008B, maxBlk 648B, frag 56%, minFree 1088B, Reboots 13.
- emergencyHeapRecovery fired: before=1456 after=3832 delta=+2376 actions=0x07 (near-OOM).
- WS client 192.168.0.102 (browser) flap-reconnect loop; WS[0]->WS[1] handoff; canSendWebSocket throttled continuously (heap=3536 maxBlock=904 ...).
- KEY: MQTT off and behaviour identical => confirms WS live-log is the heap hog independent of MQTT (supports AC#2/#3 decoupling thesis). No exception in this capture but Reboots=13.
- OUTLIER: richard_ha_ and crashevans both report 1.7.0 CLEAN. George+maintainer suspect dying/clone Wemos (he offered to swap ESP). Suggest A/B: (a) keep live-log tab closed, (b) fresh genuine Wemos — isolates SW churn vs marginal HW in one session.
<!-- SECTION:NOTES:END -->
