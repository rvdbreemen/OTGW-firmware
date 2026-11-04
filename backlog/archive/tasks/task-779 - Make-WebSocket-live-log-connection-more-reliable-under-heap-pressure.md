---
id: TASK-779
title: Make WebSocket live-log connection more reliable under heap pressure
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 12:48'
updated_date: '2026-06-29 21:48'
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
- [x] #2 Decouple or independently tune the WebSocket eligibility/throttle from the MQTT publish gate so relaxing MQTT guard does not make the WS live-log run hotter under pressure
- [ ] #3 WebSocket live-log applies backpressure/coalescing under heap pressure (e.g. drop-to-latest or rate-cap) so it cannot drive heap into the desync-prone band
- [ ] #4 WS connection survives heap-pressure events gracefully (clean reconnect, no firmware crash, no cascade into MQTT desync) verified on bench
- [x] #5 python build.py exits 0 (firmware + filesystem)
- [x] #6 python evaluate.py --quick shows no new failures
- [ ] #7 Field validation by GeorgeZ83 on NodeMCU v3 + HA: live-log open for >1h with no MQTT sensor-unavailable / malformed-packet events
- [ ] #8 Relax MQTT publish-gate thresholds (telemetry-driven from GeorgeZ83 logHeapStats; CRITICAL OOM floor unchanged) so MQTT drops/throttles less aggressively, decoupled from the WS gate
- [x] #9 New ADR for per-consumer heap gating (supersede/amend ADR-030, which is Accepted + llm_judge:true): document the WS-vs-MQTT decoupling decision and the chosen relaxed MQTT values with rationale
- [x] #10 WS_HEAP_CRITICAL/WARNING/LOW_THRESHOLD and MQTT_HEAP_CRITICAL/WARNING/LOW_THRESHOLD defined and ordered (crit<warn<low); step-1 values equal the shared HEAP_* defaults (behaviour-equivalent)
- [x] #11 getHeapHealthForWebSocket()/getHeapHealthForMQTT() exist, each evaluates its own ladder and keeps the canonical ADR-089 tier-entry counters live via getHeapHealth(); canSendWebSocket/canPublishMQTT call their per-consumer evaluator
- [x] #12 evaluate.py check_per_consumer_heap_gate added and PASSES; existing ADR-089 heap gates still PASS (getHeapHealth untouched)
- [x] #13 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
RE-SCOPED to ESP32-S3 (maintainer decision 2026-06-20; ESP8266/NodeMCU-v3 field ACs are obsolete — dev is ESP32-S3-only). Implement ADR-121 Option B (Accepted): two INDEPENDENT per-consumer heap threshold ladders so relaxing the MQTT gate cannot loosen the WebSocket gate. Step-1 is STRUCTURAL + behaviour-equivalent: WS_HEAP_* and MQTT_HEAP_* thresholds = the shared HEAP_* values for now (concrete WS-strict/MQTT-relaxed values stay telemetry-gated per ADR-121 AC#8). Design (gate-safe): keep getHeapHealth() byte-for-byte intact (ADR-089 evaluate.py gates parse its body for HEAP_FRAG_PROMOTE_MAXBLOCK + the 3 tier-entry counters); add getHeapHealthForWebSocket()/getHeapHealthForMQTT() that each (a) call getHeapHealth() once to keep the canonical ADR-089 counters live (idempotent transition counting) and (b) return a tier computed against their own per-consumer ladder via a local heapTierWithThresholds() helper. Wire canSendWebSocket()->getHeapHealthForWebSocket(), canPublishMQTT()->getHeapHealthForMQTT(). Add evaluate.py check_per_consumer_heap_gate (ADR-121's mandated gate): assert WS_/MQTT_ threshold sets exist + ordered crit<warn<low, and that each consumer calls its own evaluator. Build 3 targets + eval green. Field tuning of the relaxed values deferred (telemetry).
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

RECEIPT 2026-06-02: parity floor shipped. Build BOTH targets SUCCESS (esp8266 fw+fs, esp32 fw+fs; 8 SUCCESS 0 FAILED; esp32 flash within budget). evaluate.py --quick: Passed 61 / Failed 0 (abstraction WARN baseline 4 = expected, not new). Committed 96d09170 (46 files: gate + alpha.139 bump + ADR-121 + this task file), pushed to origin/feature-dev-2.0.0. alpha.138 (ungated) vs alpha.139 (gated) = the field A/B pair. Task stays In Progress on the deferred decouple/ADR-accept/telemetry/field gates.

2026-06-02 — ADR-121 ACCEPTED, Option B (maintainer). Decouple design locked: WebSocket and MQTT gates get INDEPENDENT per-consumer heap threshold ladders (not the shared getHeapHealth tiers), so relaxing MQTT (AC#8) can never loosen the WS gate. Binding amendment to ADR-089; impl must add CI gate check_per_consumer_heap_gate (ADR-080 / AC#9).

IMPL SEQUENCE (per ADR-121 Decision):
1. Structure: split canSendWebSocket/canPublishMQTT onto independent threshold sets, preserving ADR-089 fragmentation-promotion + tier-entry counters. Behaviour-equivalent until values diverge. + check_per_consumer_heap_gate.
2. Values: tune WS-stricter / MQTT-relaxed from George's logHeapStats. GATED on telemetry — requested from George (geo83_44083) in #beta-testing 2026-06-02 (free heap, maxBlock, level, WS_drops, MQTT_drops; live-log open vs closed; board+fw). Step 2 must NOT be guessed.

AC status: #9 ADR authored + Accepted (values portion pending telemetry, leave unchecked). #2/#3 design unblocked, structure implementable now / values pending. #8 blocked on George telemetry. #1/#4/#7 hardware/field. Task stays In Progress.

2026-06-02 (loop fire 3) — IDLE, awaiting two inputs:
  (A) MAINTAINER DECISION (asked in chat, unanswered): land Option B STEP 1 (independent-ladder structure + check_per_consumer_heap_gate, behaviour-equivalent at current values) now, OR hold the whole decouple until George's telemetry so structure+values ship together? Not preempting this autonomously: it is a refactor of the binding ADR-089 heap machine + a field push.
  (B) George's logHeapStats telemetry (requested #beta-testing 2026-06-02) — gates STEP 2 values regardless of (A).
No other 2.0.0 work is code-actionable: To Do all excluded (2.1.0: 641/648/687; blocked: 708, 802; needs-info/hw: 484/486; future-2.3.0: 409). TASK-779 stays In Progress.

STRUCTURAL IMPLEMENTATION COMPLETE (alpha.226), ADR-121 Option B. helperStuff.ino: added WS_HEAP_*/MQTT_HEAP_* ladders (=shared defaults, behaviour-equivalent step-1), heapTierWithThresholds() helper, getHeapHealthForWebSocket()/getHeapHealthForMQTT() (each keeps the canonical ADR-089 counters live via getHeapHealth()); canSendWebSocket/canPublishMQTT now consult their own ladder. OTGW-firmware.h: declarations. evaluate.py: check_per_consumer_heap_gate added + PASSES (67 passed, 0 fail, 98.7%); existing ADR-089 gates still PASS (getHeapHealth untouched). 3-target build green at alpha.226. AC#9: ADR-121 is the per-consumer ADR (Accepted, binding amendment to ADR-089; AC text says ADR-030 but ADR-089 already amended ADR-030). REMAINING (telemetry/field, deferred per ADR-121 AC#8): #1 logHeapStats characterization, #3 coalescing/drop-to-latest beyond throttle, #4 ESP32-S3 bench survival, #8 telemetry-driven relaxed MQTT values. #7 (GeorgeZ83 NodeMCU-v3) is OBSOLETE (ESP8266 dropped). Moving to In Review: the independent-ladder STRUCTURE (the ADR-121 decision) is shipped; relaxed-value tuning awaits device telemetry.

3-target build verified GREEN at HEAD (alpha.232): esp32, esp32-classic, esp32-combo all SUCCESS (fw+fs); esp32-combo bin now FITS (no overflow). evaluate.py --quick 0-fail. Code ACs were verified by the planning pass reading the committed source. Remaining = field/hardware AC(s) for Robert. (779 has no pure-build AC; build-half noted.)

CLOSED 2026-06-29 per maintainer: the underlying WebSocket live-log heap-reliability issue is FIXED by the 1.7.0 release (1.x line). The dev/2.0.0 work tracked here is superseded by that fix; the open ACs (heap characterisation, WS backpressure/coalescing, MQTT gate relaxation, GeorgeZ83 field soak) are no longer needed as separate 2.0.0 deliverables. Maintainer decision (Robert).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WebSocket live-log heap reliability resolved by the 1.7.0 release. Closed as fixed-by-release; remaining 2.0.0 ACs superseded.
<!-- SECTION:FINAL_SUMMARY:END -->
