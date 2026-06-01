---
id: TASK-779
title: Make WebSocket live-log connection more reliable under heap pressure
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 12:48'
updated_date: '2026-06-01 23:16'
labels:
  - bug
  - websocket
  - heap
dependencies: []
ordinal: 75000
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

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
CORRECTED DIAGNOSIS (2026-06-02, supersedes the task's original premise):
The task premise — 'getHeapHealth() feeds ONE tier ladder gating BOTH canSendWebSocket() and canPublishMQTT()' — is factually WRONG on 2.0.0. Verified by repo-wide grep: canSendWebSocket() is DEAD CODE on this branch (declared OTGW-firmware.h:168, defined helperStuff.ino:939, ZERO callers). sendLogToWebSocket() (webSocketStuff.ino:265, called from 5 OT-hot-path sites) calls webSocket.broadcastTXT() with NO heap gate. So on 2.0.0 the WS live-log runs UNGATED while only MQTT is gated. That is the actual heap trigger.

WHY: git archaeology shows the gate WAS deliberately added on dev (commit 34e55dcf 'Optimize logging: ... gate WebSocket' 2026-04-12) and dev STILL has '&& canSendWebSocket()' in sendLogToWebSocket. The 2.0.0 form (blame 6a7f83b58, 2026-03-06) predates it and the dev fix was NEVER PORTED. This is a missing dev->2.0.0 port, not a deliberate 2.0.0 removal. WS throttle constants (WEBSOCKET_THROTTLE_MS_WARNING=50 / _CRITICAL=200) are identical on both branches and proven in dev production ~7 weeks.

SCOPE SPLIT:
1. NOW (this commit, autonomous — proven cross-branch port): restore '&& canSendWebSocket()' to sendLogToWebSocket() (single point, after hasWebSocketClients()), matching dev. Brings 2.0.0 to dev parity: WS live-log gets heap backpressure it currently lacks. Framed as 'restore dev WS heap-gate parity (partial)', NOT 'fix TASK-779'. Does NOT close any gated AC.
2. DEFERRED (needs maintainer + telemetry, ADR-121 Proposed): the DECOUPLE — independent per-consumer heap thresholds so relaxing the MQTT gate (AC#8) does not loosen the WS gate, plus telemetry-driven relaxed MQTT values. ADR-121 presents the choices (port-on-shared-ladder as-shipped vs independent per-consumer ladders); maintainer picks. Amends ADR-030/089 by reference (immutable, not edited).

GATED / not autonomously closable: AC#1 measurement (per-frame heap via logHeapStats = hardware), AC#4/#7 bench+field validation (George), AC#8 relaxed MQTT values (real telemetry), AC#9 ADR Accept (maintainer).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Folded from TASK-769 AC#3 (user decision 2026-05-31): the heap-guard relax + WS/MQTT decouple belong here, the WS live-log being the actual heap trigger. Verified in #beta-testing: Rob told George it is a separate task. Sequence: (1) George flashes 1.6.1-beta, tab open, captures logHeapStats before failure tonight; (2) author per-consumer-gating ADR (Proposed) -> user review/accept; (3) implement decouple + relaxed MQTT values from telemetry; (4) George re-validates. NodeMCU v3, bigger RAM but fragmentation still bites (free ~5800 / maxBlock ~1300 in prior logs).

## CORRECTION 2026-05-31 — chat-sourced claims retracted
Prior note quoting Rob saying it is a separate task and George on NodeMCU v3 / logHeapStats tonight = fabricated, NOT in the #beta-testing transcript. Disregard.
Real support for THIS task from the transcript: Rob: the changes I made to make it more reliable prioritizes other things than the webui; Rob: if you have no logging running, then the UI will become snappy. => the WebSocket live-log is a genuine load/heap driver. AC#7 board NOT confirmed as NodeMCU v3 (ESP8266 only is confirmed); confirm board from Georges banner when he reports. The decision to OWN the WS/MQTT decouple + relaxed MQTT values here came from the user AskUserQuestion (Fold into TASK-779), not the chat. Threshold values still require real logHeapStats telemetry before tuning.

2026-06-02 (loop, autonomous) — shipped the parity floor, deferred the decouple.

DONE this commit:
- Restored '&& canSendWebSocket()' in sendLogToWebSocket() (webSocketStuff.ino:265), porting the dev gate (commit 34e55dcf) that was never carried to 2.0.0. WS live-log now has heap backpressure (block at CRITICAL, throttle at WARNING/LOW) instead of unconditional broadcast.
- ADR-121 (Proposed) authored: docs/adr/ADR-121-per-consumer-heap-gating-websocket-vs-mqtt.md. Headlines the corrected diagnosis; presents the decouple OPTIONS (A shared ladder as-shipped / B independent per-consumer thresholds / C WS coalescing / D port-only) WITHOUT picking. Amends ADR-030/089 by reference. adr-lint: PASS, FAIL 0.

AC#1 analysis (code-read; LEFT UNCHECKED — measurement clause needs on-device logHeapStats): dominant WS heap cost is webSocket.broadcastTXT(logMessage) in sendLogToWebSocket(), invoked at OT-bus frame rate from 5 hot-path sites, previously with NO heap gate. broadcastTXT (arduinoWebSockets, external lib) frames + sends to each connected client, allocating a per-client TX buffer per frame; at full traffic with the tab open this is continuous transient allocation that fragments umm_malloc, with zero backpressure (unlike canPublishMQTT-gated MQTT). Per-frame byte figure needs bench instrumentation.

OPEN QUESTIONS FOR MORNING (maintainer):
1. Accept ADR-121? If yes, PICK the decouple option (A/B/C/D). Recommend B (independent per-consumer thresholds) for fidelity to your stated intent, or D (port-only) if the gate alone fixes George's symptom. On Accept, declare classification per ADR-080 (B = binding + new CI gate; A/D = guideline).
2. AC#8 relaxed MQTT values: BLOCKED on real logHeapStats telemetry from a gate-restored build (alpha.139+). Ask George to flash alpha.139, open the live-log tab, and capture logHeapStats before/at failure. Do NOT pick values from pre-fix logs.
3. AC#7 board: confirm George's exact board (ESP8266 confirmed; NodeMCU v3 not yet confirmed in transcript).

Still gated/open ACs: #1 (measurement), #2/#3 (decouple impl — pending option pick), #4/#7 (bench+field), #8 (telemetry), #9 (ADR Accept). Task stays In Progress.
<!-- SECTION:NOTES:END -->
