---
id: TASK-1248
title: >-
  Detect a connected-but-dead WiFi link and recover from it, restarting as last
  resort
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 09:50'
updated_date: '2026-10-18 10:09'
labels:
  - bug
  - wifi
  - reliability
dependencies: []
priority: high
ordinal: 244000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in GH #682 / TASK-1246: during two ~72 min hangs on 1.7.7 the gateway was off the network (HTTP fails fast, MQTT 'exceeded timeout', telnet silent) while loopWifi() (networkStuff.ino:239-302) did nothing. Recovery keys only on WiFi.status() != WL_CONNECTED, and WL_CONNECTED means STATION_GOT_IP: a station that stays associated with an IP while no traffic passes is never acted on, and failed MQTT reconnects do not escalate.
Goal: recognise 'associated with IP, but the LAN is unreachable' within minutes and recover: first a WiFi reconnect (disconnect + begin), and if traffic still does not return, doRestart() with its own reason text (shows in reboot_log.txt since TASK-1247, e.g. 'WiFi: connected but no traffic').
Design questions for the plan (decide with the maintainer, KISS):
- Liveness signal. It must test the local link, not the broker: a broker that is down or misconfigured must never restart the gateway. Candidates: the default gateway answering (ARP entry refresh via lwIP etharp, or an ICMP echo through lwIP raw ping as in core's ESP8266 ping examples / an OSS lib such as ESP8266Ping, research per the reuse-OSS rule), inbound-packet counter from the netif, or 'any successful inbound TCP/UDP activity' (HTTP, telnet, NTP, MQTT) as a cheap passive signal with an active probe only when quiet.
- Thresholds: how long dead before reconnect, how many reconnect attempts before restart; no restart while isFlashing(); back-off so a dead AP does not cause a reboot loop (e.g. at most one restart per N hours, then keep reconnecting).
- Must not fire when no default gateway is configured or reachable by design (static IP without gateway).
Cross-line: decide whether 2.0.0 (ESP32-S3, different WiFi stack) needs the same guard; separate task if so.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A link that stays associated with an IP while the LAN is unreachable is detected within the agreed time, logged on telnet, and recovered by a WiFi reconnect
- [ ] #2 If the reconnect does not restore traffic, the gateway restarts via doRestart() and reboot_log.txt shows the connected-but-no-traffic reason
- [ ] #3 A broker that is down or unreachable while the LAN works does not trigger a reconnect or restart
- [ ] #4 No restart during a firmware or PIC flash, and a persistently dead AP does not cause a reboot loop (rate limit holds)
- [ ] #5 Old-vs-fix reproduction on an ESP8266 test unit (e.g. AP keeps the station associated but drops its traffic, or a firewall rule on the AP): old build stays dead, fix recovers
- [x] #6 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Research (2026-10-18):
- Liveness signal: ICMP echo to the default gateway via the SDK ping API ping_start() (lwip2 glue esp-ping.c, present in every liblwip2 variant of core 2.7.4). Asynchronous: the result arrives in sent_function with total_count and timeout_count. 1000 ms timeout per echo; coarse_time must be >= 2 s, because at 1 s the per-echo timeout and the completion timer expire together and the timeout can run after the message is freed.
- OSS candidate dancol90/ESP8266Ping (LGPL-2.1, last push 2024-02): rejected. It wraps the same ping_start() but blocks the loop until done (while(!_done) delay()) and uses coarse_time=1. Built on the SDK API directly instead (about 60 lines, no new dependency).
- Reconnect must not use WiFi.disconnect(): with persistent(true) it writes an empty SSID/password into flash (ESP8266WiFiSTA.cpp:371-388). Use wifi_station_disconnect(), which keeps the stored config; the existing loopWifi state machine then sees the drop and reconnects (and still restarts after 10 failed attempts).
Design (networkStuff.ino only):
1. While WiFi.status()==WL_CONNECTED and loopWifi is idle: every 60 s, 3 echo requests 2 s apart to WiFi.gatewayIP() (skipped when no gateway is set).
2. Learned capability: the check only acts after the gateway has answered at least once since boot. A router that blocks ICMP never arms it, and a network that is dead from boot cannot cause a reboot loop.
3. 3 consecutive probes without any reply (about 3 min): log and wifi_station_disconnect() to force a reconnect.
4. If after that reconnect 3 more probes fail: doRestart("WiFi: connected but no traffic") (reason lands in reboot_log.txt via TASK-1247).
5. Any reply resets the counters. Probe results that started before a reconnect are discarded. loopWifi already skips during isFlashing(). Broker state is never consulted.
Expected worst case: reconnect after ~3 min, restart after ~7 min, instead of ~72 min.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-18: implemented in d70247152 (networkStuff.ino/.h). Review fixes before commit: reconnect generation counter instead of a millis timestamp pair (the pair disarmed after 24.8 days without a reconnect); a probe whose SDK callback never arrives counts as dead each interval (20 s deadline) instead of disarming the check; ping_option/ping_resp verified identical between tools/sdk/include/ping.h and the lwip2 glue header the library was built with.
Evidence AC6: build.bat --firmware fresh OTGW-firmware-1.7.8-beta.2+0ebeb55.ino.bin (12:08) with 'Build completed successfully!', no networkStuff warnings (logs/build_1248b.log); evaluate.py --quick 36/36, 0 warnings.
NOT verified: AC1-AC5 need an ESP8266 unit (none on the bench, maintainer 2026-10-18). Open decision on AC4: arm-after-first-reply prevents a loop when the network is dead from boot, but a link that works a few minutes after each boot and then dies would restart about every 7 min; AC4's 'rate limit' needs either a cap or a reworded AC (maintainer to decide).
Residual: if the gateway still answers ping while the rest of the LAN cannot reach the device, this probe does not catch it.
<!-- SECTION:NOTES:END -->
