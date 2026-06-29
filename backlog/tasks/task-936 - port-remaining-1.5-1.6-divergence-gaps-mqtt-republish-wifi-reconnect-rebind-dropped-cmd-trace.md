---
id: TASK-936
title: >-
  feat(parity): port the remaining 1.5/1.6 divergence gaps to dev
  (mqtt/republish endpoint, WiFi-reconnect TCP rebind, dropped-set-command
  trace)
status: In Progress
assignee:
  - '@claude'
created_date: ''
updated_date: '2026-06-29 04:28'
labels:
  - parity
  - mqtt
  - wifi
  - 1.x-port
dependencies: []
ordinal: 150000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up from the 1.5.0/1.6.0/1.6.1 -> 2.0.0 divergence sweep (8-cluster equivalence audit, ~34 features). The 2.0.0 rewrite carried virtually all 1.5/1.6 features (worldview/sibling-suffix topics, JIT discovery, on-change publishing, static IP, bilateral OT-support map, all OT-protocol correctness fixes incl. bSlaveEchoesValue gating / proxy-answer routing / MsgID 0 gating, HA discovery, resetgateway security, /api/v2/debug, telnet banner). The timer5min jitter gap was already ported (1.x 7199e158). Three small gaps remain:

### 1. POST /api/v2/mqtt/republish endpoint (HIGH confidence, low effort)
dev HAS the internal `requestMQTTRepublishAll()` (OTGW-Core.ino:1750) — resets publish eligibility so observed OT values re-publish as first-seen — but it is only called from the reconnect path (MQTTstuff.ino:1323), NOT exposed via REST. dev DOES expose `/api/v2/discovery/republish` (re-announce configs, restAPI.ino:1780) but not the OT-value republish. 1.x v1.6.0 had `handleMqtt()` (restAPI.ino:672) for POST /api/v2/mqtt/republish. Port: add the endpoint mirroring dev's `/api/v2/discovery/republish` handler — guard on method=POST + state.mqtt.bConnected, call requestMQTTRepublishAll(), 200 JSON. Use case: force full OT value republish after a broker wipe.

### 2. WiFi-reconnect TCP listener rebind guard (HIGH confidence gap, RISKY — bench-verify first)
1.x commit 8cf181d3 "avoid re-binding TCP listeners on WiFi reconnect": on reconnect only refresh connection-dependent services (MQTT reconnect, stale WS close); do NOT re-bind TCP listeners. In dev, WIFI_RECONNECTED (networkStuff.ino:378-394) calls `startTelnet()` + `startMQTT()` + `startWebSocket()`. `startWebSocket()` is idempotent (`if(wsInitialized)return`, webSocketStuff.ino:234) and `startMQTT()` is a client reconnect (fine), but `startTelnet()` is NOT guarded — it calls `debugTelnet.begin(false)` unconditionally every reconnect (networkStuff.ino:608-616). On AsyncTCP this risks a double-bind / leaked listener on port 23.
RISK: do NOT blindly add an `if(telnetInitialized)return` guard — if dev's AsyncSimpleTelnet listener does NOT survive a WiFi-down/up cycle (netif teardown), guarding would leave telnet dead after reconnect. The 1.x maintainer determined (on ESP8266) that listeners survive; ESP32 AsyncTCP behaviour must be confirmed on the bench: force a WiFi drop/reconnect and check (a) telnet still reachable on :23, (b) no leaked listeners / bind errors in the log.

### 3. Default-stream trace for dropped MQTT set-commands (LOW priority)
1.x commit 138a517b (#602) added two default-stream `DebugTf` lines so silently-dropped set-commands surface without enabling the MQTT debug toggle: `MQTT command [%s] dropped: no PIC detected` and `... no matching OTGW command (check topic spelling)`. dev's MQTT set-command dispatch (MQTTstuff.ino ~855+) is fully rewritten (SAT dispatch tables, etc.) and logs via the gated `MQTTDebugf`/`MQTTDebugTf` stream. Low value and dev's dispatch differs; verify whether the OTGW-command (non-SAT) reject path emits anything at default level, and add a `DebugTf` only if genuinely silent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 POST /api/v2/mqtt/republish added (mirrors /api/v2/discovery/republish): method+connected guards, calls requestMQTTRepublishAll(), 200 JSON; builds clean for esp32
- [ ] #2 WiFi-reconnect TCP rebind: bench-confirm whether dev's AsyncSimpleTelnet listener survives a WiFi drop/reconnect; if it does, guard startTelnet() (and remove the redundant reconnect rebind) per 1.x 8cf181d3; if not, document why the rebind stays
- [x] #3 Dropped-set-command default-stream trace (#602): verify dev's OTGW-command reject path; add a DebugTf only if it is genuinely silent at the default debug level
- [x] #4 timer5min jitter ported (1.x 7199e158): added 30000,60000 jitter params to DECLARE_TIMER_MIN(timer5min,...) so the 5-min publish desyncs from timer60s (avoids the joint-fire heap spike); builds clean for esp32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#3 VERIFIED (no code needed): dev's OTGW-command reject path already emits the two #602-equivalent traces at DEFAULT (ungated DebugTf) level — MQTTstuff.ino:944 'dropped: no OT command interface available' (generalises 1.x 'no PIC detected' for PIC+OTDirect) and :1007 'dropped: no matching OTGW command (check topic spelling)'. Only SAT-specific unknown sub-commands use the gated MQTTDebugTf. Nothing silent; AC#3 satisfied by existing code.

ON-DEVICE 2026-06-29 (OTGW32 @192.168.88.39, alpha.285): AC#1 republish PASS — POST /api/v2/mqtt/republish -> 200 {status:republish_requested} (MQTT connected); immediate 2nd POST -> 429 {Republish cooldown active, retry in 60s} (CWE-770 guard verified); GET -> 405 (method-not-allowed, route exists). AC#2 (WiFi-reconnect TCP rebind) still bench-gated/untested here; task stays open on AC#2 only.
<!-- SECTION:NOTES:END -->
