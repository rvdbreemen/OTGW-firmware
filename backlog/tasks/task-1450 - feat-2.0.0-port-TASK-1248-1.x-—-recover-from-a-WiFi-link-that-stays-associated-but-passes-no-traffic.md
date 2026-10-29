---
id: TASK-1450
title: >-
  feat-2.0.0: port TASK-1248 (1.x) — recover from a WiFi link that stays
  associated but passes no traffic
status: To Do
assignee: []
created_date: '2026-10-29T06:21:00Z'
updated_date: '2026-10-29T06:30:00Z'
labels:
  - port-from-1.x
  - wifi
  - network
  - reliability
  - low-priority
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 610000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of the 1.x connected-but-dead WiFi detection (1.x TASK-1248, investigation TASK-1246, GH #682) to the 2.0.0 line. Low priority: no 2.0.0 field report of the symptom exists, but the supervisor gap is identical.

## Problem (both lines)
loopWifi() only acts when WiFi.status() != WL_CONNECTED. WL_CONNECTED means associated with an IP, so a station that stays associated while the AP or router drops its traffic is never recovered. On 1.x (GH #682) a gateway was unreachable twice for about 72 minutes this way. origin/dev has the same gap: src/OTGW-firmware/networkStuff.ino:344-349 (WIFI_IDLE) and :410-411 (WIFI_FAILED restarts unconditionally). git grep for raw_new / IP_PROTO_ICMP / esp_ping / dead-link on origin/dev finds nothing.

## 1.x implementation (reference, branch otgw-1.x.x)
Commits: d70247152 (first version), a3f82891d (own raw ICMP pcb instead of SDK ping_start, REST witness, doRedirect reason), 0d9381c0a (refused send counts as unanswered), a8c79fbf6 (hardening after review: witnesses, 8 probes, cap via WIFI_FAILED, per-gateway arming, RTC count captured at top of setup()), 1e2531f7e + revert 1c3db99f3 (MQTT 3-minute fast path, REVERTED: MQTT loss is not independent when the broker path crosses the router). Final code: src/OTGW-firmware/networkStuff.ino:503-862.
Mechanism:
- wifiLinkProbeTick() (networkStuff.ino:735-781), called from WIFI_IDLE while connected: every 60 s, if getHeapHealth()==HEAP_HEALTHY and WiFi.gatewayIP() is set, create a raw ICMP pcb, send 3 echo requests (ID 0x4F54, 16 data bytes, own seq base) 2 s apart from the loop, judge 2 s after the last, remove the pcb. No lwIP timers, so a probe cannot get stuck. A pcb or pbuf allocation failure skips the probe (not counted); a refused send counts as unanswered.
- wifiEchoRecv() (617-631): reads the ICMP header with pbuf_copy_partial(), eats only echo replies with its own ID from the probe target, never moves p->payload (the SDK ping_start receive glue did, which dropped inbound echo requests to the gateway for ~6 s each minute).
- handleWifiProbeVerdict() (658-719): arm only after the current gateway answered once (a router that blocks ICMP never arms; a changed gateway address disarms). After 8 dead probes in a row: if a witness proves the LAN works (MQTT enabled and connected, a REST request within 60 s, or a port-25238 client byte within 60 s) do nothing; else first a forced reconnect (credentials kept), and if the next 8-probe streak is dead too, doRestart("WiFi: connected but no traffic").
- Restart cap: count in RTC memory, survives software restarts only; at 3 no more restarts, at most one forced reconnect per 15 min; cleared after 60 min of answered probes. A forced reconnect that ends in WIFI_FAILED counts toward the cap, and at the cap WIFI_FAILED retries instead of restarting (850-862).
- wifiReconnectGen bumped in WIFI_RECONNECTED (843) so a probe that spans a reconnect is discarded; alive window restarts after every reconnect.
- Witness helpers: restRequestWithin() in restAPI.ino (timestamp set at the top of processAPI), otgwStreamRxWithin() in OTGW-Core.ino (netFloorLastByteMs).

## Where it goes on origin/dev
- src/OTGW-firmware/networkStuff.ino: new probe block before loopWifi() (:326); call the tick from WIFI_IDLE (:344-349) when connected; bump a reconnect generation in WIFI_RECONNECTED (:388-406); cap logic in WIFI_FAILED (:410-411).
- Witnesses: state.mqtt.bConnected gated on settings.mqtt.bEnable (MQTTstuff.ino:280 accessor); REST timestamp at the top of processAPI(AsyncWebServerRequest*) restAPI.ino:2744 (runs on async_tcp: single aligned uint32_t store, read on the loop task); port-25238 RX timestamp in the OTGWstream read loop OTGW-Core.ino:5782-5788.
- Heap gate: getHeapHealth() helperStuff.ino:910-921. Restart: doRestart() helperStuff.ino:627 (already records a reason in reboot_log.txt via the dev reboot path; confirm the reason string reaches the log).
- Persisted count: RTC_NOINIT_ATTR struct {magic, count}, read once early in setup(); keep it only when esp_reset_reason()==ESP_RST_SW.

## Platform considerations (ESP32-S3, arduino-esp32 3.x)
- lwIP runs on the tcpip task. If the raw pcb approach is kept, wrap raw_new/raw_recv/raw_sendto/raw_remove in LOCK_TCPIP_CORE()/UNLOCK_TCPIP_CORE() (CONFIG_LWIP_TCPIP_CORE_LOCKING=y, see src/libraries/Platform/src/platform_esp32.h:335-349) and make the reply mask std::atomic or volatile; the callback runs on the tcpip task. Put the platform-specific part behind a Platform-library helper if that fits the existing pattern.
- Alternative: ESP-IDF ping_sock (esp_ping_new_session). Evaluate on evidence (does it disturb inbound ICMP, task stack and heap cost, callbacks on its own task) and record the choice in the task notes. Do not assume the 1.x reason to avoid the SDK ping applies; it was specific to the ESP8266 lwip2 glue.
- Forced reconnect: WiFi.disconnect(false, false) or esp_wifi_disconnect(); never erase stored credentials. The existing WIFI_IDLE branch then sees the drop and reconnects.
- OTGW32 Ethernet: loopWifi() returns early when state.net.eMode==NET_ETHERNET (networkStuff.ino:335); the probe must not run on a wired link and must reset its state on a transport switch.
- Prerelease builds go to AP fallback after 2 failed retries (networkStuff.ino:366-377) instead of WIFI_FAILED; make the cap and forced-reconnect logic coherent with that path (a forced reconnect must not drop a working-LAN gateway into AP mode within a minute of a router reboot).
- AsyncTCP: a WiFi disconnect closes every async client (web, WS, telnet, 25238); this is why the witnesses come first.
- loopWifi() is already skipped while isFlashing() (OTGW-firmware.ino:1048), so no restart during OTA or PIC flash; keep it that way.

## Risks
- A forced reconnect releases the DHCP lease; if the silent router is also the DHCP server the gateway stays offline until it returns. Accepted on 1.x because the 8-probe threshold and the witnesses mean nobody loses access they had.
- False verdict on routers that rate-limit ICMP: covered by per-gateway arming plus 8 consecutive dead probes.

## Do NOT port
- The reverted MQTT fast path (1e2531f7e, reverted in 1c3db99f3).
- ESP8266 specifics: ESP.rtcUserMemory offsets 113..114, REASON_SOFT_RESTART, wifi_station_disconnect(), wifiKeepRadioAwake(), the esp-ping.c workaround rationale.
- The 1.x doRedirect() reason refactor (FSexplorer.ino, a3f82891d) and the TASK-1278 wifiSelfDisconnect banner flag, unless dev has the matching feature; they are separate items.

## Evidence that dev lacks it

- origin/dev (b07e3194a) src/OTGW-firmware/networkStuff.ino:344-349: WIFI_IDLE only checks WiFi.status() != WL_CONNECTED, no liveness probe (same gap TASK-1246 found on 1.x)
- origin/dev src/OTGW-firmware/networkStuff.ino:410-411: WIFI_FAILED restarts unconditionally, no dead-link cap
- git grep -i 'dead.link|deadlink|raw_new|IP_PROTO_ICMP|ping_start|esp_ping|icmp|no traffic' origin/dev -- src/OTGW-firmware: no hits; gatewayIP only used for display (networkStuff.ino:255, restAPI.ino:3149)
- git grep -i 'dead.link|ping the gateway|#682|connected but no traffic' origin/dev -- backlog/tasks backlog/archive docs: only unrelated hits (TASK-1432 doc links, ADR-186:129 mentions #682 for the MQTT send-timeout, not the WiFi hang)
- origin/dev src/libraries/Platform/src/platform_esp32.h:335-349: build has CONFIG_LWIP_TCPIP_CORE_LOCKING=y and already uses LOCK_TCPIP_CORE()
- origin/dev witnesses available: state.mqtt.bConnected (MQTTstuff.ino:280), processAPI entry restAPI.ino:2744, port 25238 RX loop OTGW-Core.ino:5782-5788, getHeapHealth()/HEAP_HEALTHY (helperStuff.ino:910-921), doRestart helperStuff.ino:627, loopWifi skipped while isFlashing() (OTGW-firmware.ino:1048)

## Platform notes

ESP32-S3/arduino-esp32 3.x: lwIP runs in its own tcpip task, so raw_new/raw_recv/raw_sendto/raw_remove must run under LOCK_TCPIP_CORE() (core locking is on, platform_esp32.h:335) and the recv callback runs on the tcpip task, so reply flags must be atomic/volatile. Alternative: ESP-IDF ping_sock (esp_ping_new_session) runs on its own task over a socket; it does not have the esp-ping.c payload-shift bug, but its effect on inbound ICMP and its task/heap cost must be checked before it is chosen. Persisting the restart count: no ESP.rtcUserMemory; use an RTC_NOINIT_ATTR struct with a magic and esp_reset_reason()==ESP_RST_SW (panics are ESP_RST_PANIC, so the clear rule is cleaner than on 1.x). Forced reconnect: WiFi.disconnect(false,false) or esp_wifi_disconnect(), never erase credentials. Interaction with OTGW32 Ethernet: loopWifi returns early when state.net.eMode==NET_ETHERNET (networkStuff.ino:335), the probe must stay off then. Interaction with the prerelease AP fallback (WIFI_CONNECTING goes to WIFI_AP_FALLBACK after 2 retries under _VERSION_PRERELEASE, networkStuff.ino:366-377): the cap logic for WIFI_FAILED does not apply in that build. AsyncTCP: a disconnect drops every async client; witnesses must be read on the loop task (processAPI runs on async_tcp, so the timestamp must be a single atomic 32-bit store).

## Reviewer corrections (applied; second-reader findings from the port evaluation)

The verdict 'missing' holds. Every dev line I opened (b07e3194a) matches the analyst's claims, and I found no probe or port task on dev under another name. One finding the analyst missed strengthens the case for porting and adds a design caveat. Dev TASK-1162 recorded associated-but-deaf windows on ESP32-S3 itself (165 s to 273 s, no RX reaching lwIP). The AP's own disconnect ended them, not the firmware. So 'no 2.0.0 report of the symptom' is not quite right: there is bench evidence from dev, although the cause there (an internal-DRAM deadlock under a request storm) differs from the AP-side drop in GH #682. Caveat: in that deadlock state, the probe's own pbuf/pcb allocations and the getHeapHealth() gate will usually skip the probe, which by design does not count as dead. So the port will NOT recover the TASK-1162 class of deafness. That is acceptable, but the task should state it so nobody assumes otherwise.

1) In the Problem section, add: 'Related 2.0.0 evidence: dev TASK-1162 (implementation notes, run 2026-10-04/05) observed the ESP32-S3 station staying associated but deaf for 165-274 s under a request storm, ended only by an AP-side disconnect (reason 16/2). Different root cause (DRAM deadlock, not AP-side traffic drop). The probe is skipped while heap health is not HEALTHY or a pbuf allocation fails, so this port is not expected to fix that class; do not claim it does.' 2) Soften 'no 2.0.0 field report of the symptom exists' to 'no 2.0.0 field report of the GH #682 symptom; a related deaf-but-associated state was seen on the bench (dev TASK-1162)'. 3) Optional extra AC: during a TASK-1162-style storm, the probe neither counts skipped probes as dead nor triggers a reconnect (a skipped probe is logged as skipped). This keeps the port from turning a transient overload into a forced WiFi drop. All other file:line targets checked (networkStuff.ino WIFI_IDLE ~344-349, WIFI_RECONNECTED ~388-406, WIFI_FAILED ~410-411, Ethernet early return ~335, prerelease AP fallback ~366-377, helperStuff.ino:627/909-921, restAPI.ino:2744, platform_esp32.h:335-349) are correct.

Source: port evaluation 2026-10-29 (1.x v1.7.7..otgw-1.x.x 76a89bc3d against origin/dev b07e3194a).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On a 2.0.0 WiFi build, a link that stays associated while the router drops the gateway's traffic (MikroTik firewall/fault-injection rule on 192.168.88.1, association kept) is detected and logged on telnet, recovered by a forced WiFi reconnect after 8 consecutive dead probes (about 8 min), and, if traffic still does not pass, ended by doRestart with reason 'WiFi: connected but no traffic' visible in reboot_log.txt
- [ ] #2 Red/green bench proof on .63 (Classic+S3) or .61 (OTGW32 on WiFi): current origin/dev build stays unreachable for the full test window (at least 25 min), the fix recovers; commands, timestamps and telnet excerpts recorded in the task notes
- [ ] #3 No action while a witness shows LAN traffic: with MQTT connected, with the web UI polling REST, or with a port-25238 client sending, a silent gateway does not cause a reconnect or restart (bench check with ICMP to the gateway blocked but LAN traffic allowed)
- [ ] #4 A router that never answers ICMP never arms the check (no reconnect over at least 15 min), and a changed gateway address disarms it
- [ ] #5 Restart cap holds: after 3 dead-link restarts the gateway stops restarting and forces at most one reconnect per 15 min; the count survives a software restart (RTC_NOINIT + esp_reset_reason()==ESP_RST_SW), clears on power-on or panic and after 60 min of answered probes; a forced reconnect that ends in WIFI_FAILED counts toward the cap
- [ ] #6 The probe never runs while isFlashing(), while heap health is not HEALTHY, or while the active transport is Ethernet (OTGW32 wired); a switch WiFi to Ethernet and back resets probe state
- [ ] #7 Inbound ping to the gateway is not disturbed by the probe: a 30 min external ping at 1 s interval shows no loss attributable to probe windows
- [ ] #8 lwIP access is thread-safe: raw pcb calls under LOCK_TCPIP_CORE (or ping_sock chosen with the reasoning recorded); no new TWDT or assert under a 30 min WS + REST load with the probe active
- [ ] #9 Build for esp32, esp32-classic and esp32-combo green and python evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Do together with TASK-1455 (both touch the WiFi supervisor), per the port evaluation.
<!-- SECTION:NOTES:END -->
