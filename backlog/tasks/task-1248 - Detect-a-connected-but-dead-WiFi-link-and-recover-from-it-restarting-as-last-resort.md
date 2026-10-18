---
id: TASK-1248
title: >-
  Detect a connected-but-dead WiFi link and recover from it, restarting as last
  resort
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-18 09:50'
updated_date: '2026-10-18 13:06'
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

2026-10-18 adversarial review (workflow wf_dac64536-9e0: 5 lenses, 3 refuters per finding; synthesis and 11 note-level verifies lost to the session limit). Upheld and fixed in the working tree (NOT yet built or committed: the build was stopped by Claude Code for low system memory):
- [major, 3/3] Gateway-only verdict cut working LAN sessions (stopAll on disconnect, ESP8266WiFiGeneric.cpp:233), incl. an OTmonitor PIC upgrade on 25238 that isFlashing() misses. Fix: second witness, no action while state.mqtt.bConnected (PubSubClient drops a dead link within 2 keepalives, PubSubClient.cpp:373-377) or a 25238 client sent a byte within 60 s (new otgwStreamRxWithin(), OTGW-Core.ino).
- [2/3] Cap bypassed via WIFI_FAILED after a forced reconnect. Fix: WIFI_FAILED counts toward the cap when wifiDeadReconnectDone; at the cap it keeps retrying instead of restarting.
- [3/3] Stuck probe at the cap reconnected every 3 min forever; the related 'cap reconnect loop' finding was refuted 1/3 but the mechanism is real on a false verdict. Fix: at the cap at most one forced reconnect per 15 min.
- [3/3 + 2/3] NULL raw pcb in ping_start under heap exhaustion (esp-ping.c:277-281, LWIP_NOASSERT cc.h:81). Fix: no probe unless getHeapHealth()==HEAP_HEALTHY (skipped, not counted).
- [3/3] Arming per boot, not per gateway. Fix: armed gateway address stored; a changed gateway disarms.
- [3/3] Alive window not restarted by a reconnect. Fix: wifiLinkAliveSinceMs = millis() in WIFI_RECONNECTED.
- [3/3] Dead-link count and TASK-1247 reason read too late (after startWiFi, whose portal timeout restarts bare). Fix: captureRebootReason() and captureWifiDeadRestarts() at the top of setup(); the count survives software restarts (no zeroing at boot), clears on power-on/crash/watchdog and after 60 min alive; the portal timeout stores 'WiFi: config portal timeout'.
- [6/6] RTC comment wrong: eboot uses user offsets 0..31 (SDK blocks 64..95), not 64..95. No overlap with 96..97/100..112/113..114. Comments corrected.
- [notes] 20 s deadline was effectively one interval: constant removed, comment says so. SDK scores replies > 1 s as timeouts: documented.
Scenario walk (a)-(k) against the fixed code: (a) healthy: armed, counters stay 0; (b)+(c) death then recovery: 3 dead probes (worst case 3 more if MQTT still within its 120 s keepalive), forced reconnect, reset on first answer; (d) reconnect OK but dead: 3 more dead probes, restart 'connected but no traffic'; (e) reconnect cannot associate: WIFI_FAILED counted restart, then the pre-existing portal loop (ADR-047, uncapped, now with its own reason); (f) flapping: 3 restarts, then capped reconnects every 15 min, clear after an hour alive; (g) ICMP-silent router: never armed; (h) partial loss: any reply counts alive; (i) stuck probe: dead each interval, witness suppresses, else reconnect then restart (fresh SDK); at the cap the check is effectively off until reboot; (j) millis rollover: unsigned diffs and a generation counter, no timestamp compare left; (k) gateway change: disarm and re-arm on the new address.
AC deviations (protocol in docs/guides/backlog-cli.md): AC1 'logged on telnet' cannot be observed during the fault (the link is dead and stopAll closes telnet); observable evidence is the capture's HTTP/MQTT gap, the AP station log and reboot_log.txt. AC2 holds for the counted path; a forced reconnect that cannot reassociate restarts with 'too many reconnect failures' (also counted). AC4: the cap covers dead-link and forced-reconnect restarts; the config-portal timeout loop when the network is dead at boot is pre-existing (ADR-047) and out of scope. Open for the maintainer: WIFI_PROBE_FAILS_TO_ACT (3). Without a live local broker as witness, a router reboot > 3 min now forces a disconnect; if the router is also the DHCP server the lease is lost and the WIFI_FAILED restart follows 5 min later (resets the PIC). 8-10 keeps typical router reboots harmless, #682 recovers in ~10-15 min instead of ~7.

2026-10-18: review fixes committed in a8c79fbf6 (pushed). Maintainer chose the threshold: WIFI_PROBE_FAILS_TO_ACT = 8 (router reboots of a few minutes pass unharmed; #682 recovers in ~8 min by reconnect, ~17 min if a restart is needed).
Evidence AC6 (re-run on the final code): build.bat --firmware fresh OTGW-firmware-1.7.8-beta.2+032f42f.ino.bin (774528 B, 14:21) with one 'Build completed successfully!' line and no warnings/errors in the changed files (logs/build_1248e.log); evaluate.py --quick exit 0, 36 passed, 0 warnings, 0 failed (logs/eval_1248e.txt). Commit hook: adr-judge 0 violations.
Still open, hardware needed (no ESP8266 on the bench): AC1-AC5. Also not re-verified after the fixes: the review itself ran on the pre-fix code; a small delta review of a8c79fbf6 is still worth doing when the session limit allows.

2026-10-18 delta review of 19a9d444c (workflow wf_57f0d502-b10, 3 lenses + 2 refuters each, 21 agents, all upheld findings 2/2):
- [major] SDK ping_start breaks inbound ICMP: the linked core 2.7.4 glue (tools/sdk/lwip2/builder/glue-lwip/esp-ping.c:142) shifts p->payload by PBUF_IP_HLEN and returns 0 for packets it does not eat, so lwIP strips the IP header twice and drops echo requests to the gateway for ~6 s of every minute. Fixed in a3f82891d: own lwIP raw ICMP probe, payload never touched, pcb only during the probe, loop-driven timing (no lwIP timers, no stuck probe).
- [minor] stale state.mqtt.bConnected after a runtime MQTT disable kept the witness true for good. Fixed in a3f82891d (gated on settings.mqtt.bEnable).
- [minor] at the cap the first forced reconnect of a streak bypassed the 15 min limit. Fixed in a3f82891d.
- [minor] web UI reboot and WiFi reset logged as 'Reboot after upgrade'. Fixed in a3f82891d (doRedirect takes the reason).
- [minor, accepted residual] a forced reconnect releases the DHCP lease (lwip-git.c dhcp_release_and_stop on netif down); if the silent gateway is also the DHCP server, the gateway stays offline until it returns. Mitigated by the 8-probe threshold and three witnesses (MQTT, REST, port 25238): it only happens when no LAN client is talking to the gateway, so nobody loses access they had.
- Seed walk notes: at the cap a failed forced reconnect keeps retrying instead of restarting, so the config portal is not offered again until a power cycle (per spec). Seeds 1-5 acceptable or not reachable.
- [note, inferred, not verified] core 2.7.4 restarts after panic/abort() through the SDK (core_esp8266_postmortem.cpp:113-124, 215), so such a crash probably reads as REASON_SOFT_RESTART: it keeps a pending reason/count, and on 1.7.x a '(4)' entry without a reason may also be a panic. Stated in the code comment; not in public text.
<!-- SECTION:NOTES:END -->
