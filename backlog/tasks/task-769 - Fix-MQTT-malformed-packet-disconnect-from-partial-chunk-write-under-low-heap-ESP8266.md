---
id: TASK-769
title: >-
  Fix: MQTT malformed-packet disconnect from partial chunk write under low heap
  (ESP8266)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-30 21:42'
updated_date: '2026-05-31 12:48'
labels:
  - bug
dependencies: []
references:
  - >-
    Discord #english-support / GeorgeZ83 (geo83_44083) / 2026-05-29..30 / logs:
    putty-telnet.log + log.txt + mosquitto broker log
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by GeorgeZ83 (geo83_44083) in Discord #english-support, 2026-05-29/30. ESP8266 OTGW fw 1.6.0+0542da1, MQTT to Home Assistant. Symptoms: HA sensors go unavailable, web UI freezes, broker drops the client with "malformed packet" then "session taken over".

Three-source evidence (cross-correlated):
1. Mosquitto broker log (George): client OTGWC8C9A35ACB08 disconnected: malformed packet at 23:45:23 and 23:51:21; session taken over at 00:01:34.
2. putty-telnet.log: matching MQTT outage, handleMQTT reconnect 23:52:03 after "offline 42044ms" — exactly the gap between the 23:51:21 malformed-packet drop and reconnect.
3. Heap critically low when web UI WebSocket live-log open: banner minFree 2312, logHeapStats dips to 5848 free / maxBlock 1296; WS churn ws_drops 15->26 from browser at .102.

Root cause (code-confirmed, MQTTstuff.ino): the streaming publish path beginMqttPublish()->writeMqttChunk()->endPublish() commits a fixed MQTT remaining-length in beginPublish(topic,len). writeMqttChunk (MQTTstuff.ino:283) bails when MQTTclient.write() short-writes (TCP sndbuf exhausted at low heap), but the caller (sendMQTTData MQTTstuff.ino:1004-1008, 1052-1056; sendMQTT 1230) then still calls MQTTclient.endPublish(). A header promising len bytes followed by fewer bytes desynchronises the MQTT TCP stream -> broker parses the next packet header as payload -> "malformed packet" -> disconnect. Reconnect with the same client-id while the old session lingers -> "session taken over". This matches the maintainer hypothesis (chunked streaming replaced the 1.2.0 buffered publish; pubsubclient unchanged).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 writeMqttChunk/writeMqttProgmemChunk short-write no longer leaves a partial MQTT packet on the wire (broker never sees malformed packet): on unrecoverable short-write the TCP connection is cleanly dropped (MQTTclient.stop) instead of calling endPublish() on a truncated payload
- [x] #2 Add a bounded retry-with-yield on MQTTclient.write() short-writes so a started publish completes when lwIP sndbuf drains, rather than aborting on the first short write
- [x] #3 Heap-guard threshold review: document the trade-off (lower guard = fewer drops but more partial-write disconnects) and only relax HEAP_LOW/HEAP_WARNING after the desync fix lands; record chosen values with rationale
- [ ] #4 python build.py --firmware exits 0
- [x] #5 python evaluate.py --quick shows no new failures
- [ ] #6 Field validation by GeorgeZ83 on ESP8266 + HA: no malformed-packet/session-taken-over disconnects with web UI open
- [x] #7 Discovery composer path (mqtt_configuratie.cpp stream*Discovery, 7 sites): failure branch must drop TCP via client.disconnect() instead of client.endPublish() on a truncated payload — this is the largest-payload path, most prone to short-write desync under heap pressure
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. writeMqttChunk + writeMqttProgmemChunk (MQTTstuff.ino:283,300): add bounded retry-with-yield() on short MQTTclient.write() so a started publish completes when lwIP sndbuf drains. [done in 7e5a61ed]
2. On genuine failure (retries exhausted): callers (sendMQTTData 1004-1008, PROGMEM overload 1052-1056, sendMQTT 1230) must drop the TCP link via MQTTclient.disconnect() instead of endPublish() on a truncated payload. [done in 7e5a61ed]
3. Discovery composers in mqtt_configuratie.cpp: replace endPublish() failure branches with client.disconnect() on truncated payload. [done in f8314a0b]
4. Field-validation support: fix scripts/capture-mqtt-debug.ps1 interactive mode to ask for an optional MQTT username and a secure password when credentials are needed; keep blank username as anonymous mode. Validate by capturing the mosquitto_sub argument vector with a local fake executable and fake telnet device.
5. Build/evaluate validation remains firmware-scoped; for script-only follow-up run syntax and harness validation before committing the script change. Do not mark TASK-769 Done until GeorgeZ83 field validation confirms no malformed-packet/session-taken-over disconnects.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Related prior work: TASK-242 (serial overrun + MQTT throttle drops, Done).

Heap tiers at time of report (helperStuff.ino:900-902): CRITICAL=1536 (block), WARNING=3072 (throttle 500ms), LOW=5120 (throttle 100ms); frag-promote when freeHeap in LOW band and maxBlock<1536. Throttle MS: helperStuff.ino:915-918.

User request under evaluation: lower heap-guard thresholds to reduce MQTT drops. RISK: relaxing the guard BEFORE the desync fix makes publishes proceed at lower heap -> more short-writes -> MORE malformed-packet disconnects. Correct order = fix writeMqttChunk desync first, then relax guard.

Desync fix implemented + committed 7e5a61ed (MQTTstuff.ino only).
- writeMqttChunk/writeMqttProgmemChunk: bounded retry-with-yield (MQTT_WRITE_MAX_RETRIES=10) on short MQTTclient.write().
- 3 caller failure branches: MQTTclient.disconnect() instead of endPublish() on truncated payload (agent first used non-existent PubSubClient.stop(); corrected to disconnect()).
- Build: python build.py exit 0. Evaluate --quick: 34/34 pass, 0 fail, 100%.
- NOT pushed (field validation by George pending). Guard-relax deferred.

Discovery-path gap found + fixed (2nd commit f8314a0b, mqtt_configuratie.cpp +7/-7). 7 stream*Discovery composers (sensor/binsensor/number/climate-ish/button/select) called client.endPublish() on writer.ok==false (truncated) -> same desync. Now client.disconnect(). This is the largest-payload path, most likely to short-write under George heap pressure. Build exit 0, eval 100%, adr-judge 0 violations. Dev TASK-769 commits: 7e5a61ed + f8314a0b. Not pushed (George field validation pending).

2026-05-31 @codex follow-up: user reported scripts/capture-mqtt-debug.ps1 interactive mode never asks for MQTT username/password, so mosquitto_sub runs anonymously and field capture fails on authenticated brokers. Plan: add optional interactive username prompt; when a username is provided interactively or via -Username without -Password, prompt securely for password; keep anonymous brokers working when username is blank; validate with a non-network harness that captures mosquitto_sub arguments and summary output.

2026-05-31 @codex scope update: add script usability hint too. User requested help discoverability (e.g. --help) and explicit instructions for stopping the capture manually. Include this in scripts/capture-mqtt-debug.ps1 and the .bat launcher path while preserving the credential fix.

2026-05-31 @codex script follow-up implemented. scripts/capture-mqtt-debug.ps1 now prompts for an optional MQTT username during interactive host-prompt mode and prompts securely for the password when a username is supplied without -Password. Blank username keeps anonymous mode. Added -Help output with usage, credential behavior, and Ctrl+C/-DurationSeconds stop instructions; scripts/capture-mqtt-debug.bat maps --help and /? to -Help. Validation: pwsh parser OK; ps1 -Help OK; ps1 --help OK; bat --help OK; explicit credential harness passed (-u mqttuser -P mqttpass, password not in summary); interactive Read-Host shim passed for credential mode; anonymous interactive shim passed with no -u/-P.

## Wrap-up 2026-05-31

Desync fix landed + pushed both branches (dev e5a26192 status path + 2de244f2 discovery path; 2.0.0 dabc6f71 + 4363246f). Root cause triple-confirmed via Discord #beta-testing (George + Rob + Sergeant D): garbage tail DIFFERENT each decode-failure, always a discovery config payload = truncated-publish desync, not framing bug, not heap corruption. The G was a red herring (0x83 = SUBSCRIBE nibble, impossible mid-publish).

### AC#3 decision: defer guard relax to telemetry-driven tuning
Trade-off documented: lower guard = fewer throttle-drops but more (now-graceful) partial-write disconnects. KEY new signal from Discord: web UI live-log (WebSocket OT-frame push) is the heap-pressure TRIGGER (George: tab closed = stable for hours; open = sensors unavailable in ~10 min). getHeapHealth() feeds ONE tier ladder gating BOTH canSendWebSocket() and canPublishMQTT(). Lowering the shared HEAP_LOW/HEAP_WARNING would relax the WS live-log too, raising heap pressure at exactly the level George crashes = worsens his trigger. CHOSEN VALUES: keep current (CRITICAL 1536 / WARNING 3072 / LOW 5120 / RESTORE 6144) for now; do NOT relax blind. Tune with real heap-dip telemetry from Georges repro after the desync-fix beta. The WS-reliability root focus spun off to TASK-779 (decouple WS throttle from MQTT gate, backpressure live-log, robust reconnect).

### Remaining
AC#6 = field validation by GeorgeZ83 (NodeMCU v3 + HA, live-log open). Hardware-gated, cannot self-verify. George has a reliable 10-min repro and volunteered to hammer the beta. Task stays In Progress until he confirms no malformed-packet / session-taken-over events.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix MQTT malformed-packet disconnect caused by truncated-publish desync under heap pressure on ESP8266.

## Root cause
Streaming publish path beginPublish(topic,len) -> writeMqttChunk -> endPublish() commits a fixed MQTT remaining-length. Under low heap, MQTTclient.write() short-writes; the chunk helper bailed but callers still called endPublish() on a truncated payload. Broker then parses the next packet header as payload tail -> malformed packet -> disconnect -> reconnect with same client-id -> session taken over -> HA sensors unavailable + web UI freeze. Triple-confirmed in Discord #beta-testing: decode-failure garbage tail differs every time and always involves a discovery config payload = desync, not a framing bug or heap corruption.

## Changes
- writeMqttChunk / writeMqttProgmemChunk: bounded retry-with-yield (MQTT_WRITE_MAX_RETRIES=10) so a started publish completes when lwIP sndbuf drains instead of aborting on first short write (AC#2).
- Status publish path (MQTTstuff.ino, 3 sites) + discovery composer path (mqtt_configuratie.cpp stream*Discovery, 7 sites): on unrecoverable short-write, drop the TCP connection via MQTTclient.disconnect() instead of endPublish() on a truncated payload, so the broker never sees a malformed packet (AC#1, AC#7). Discovery path carries the largest payloads and is most desync-prone.
- Pushed: dev (e5a26192 + 2de244f2), 2.0.0 sibling TASK-770 (dabc6f71 + 4363246f). Both branches green at commit time.

## AC#3 heap-guard relax: deferred by design
Documented the trade-off. New Discord signal: the WebSocket live-log is the heap-pressure trigger, and the heap tier ladder gates both WS and MQTT. Relaxing the shared guard would make the WS live-log run hotter = worsen the trigger. Chosen values: keep current thresholds; tune later with real telemetry from Georges repro. WS-connection-reliability root focus spun off to TASK-779.

## User impact
MQTT corruption + session-takeover replaced by a clean brief reconnect (George ratified this trade-off: prefers reconnect over corrupt sensors). No firmware behaviour change at healthy heap.

## Tests
- python build.py (firmware + filesystem): green
- python evaluate.py --quick: no new failures (baseline 0 unsafe patterns)

## Risks / follow-ups
- AC#6 field validation by GeorgeZ83 (NodeMCU v3 + HA, live-log open) pending = task stays In Progress until confirmed. George has a 10-min repro and volunteered.
- TASK-779: make WebSocket live-log connection reliable under heap pressure (the actual trigger).
<!-- SECTION:FINAL_SUMMARY:END -->
