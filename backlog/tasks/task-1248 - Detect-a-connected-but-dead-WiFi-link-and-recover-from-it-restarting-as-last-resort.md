---
id: TASK-1248
title: >-
  Detect a connected-but-dead WiFi link and recover from it, restarting as last
  resort
status: To Do
assignee: []
created_date: '2026-10-18 09:50'
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
- [ ] #6 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
