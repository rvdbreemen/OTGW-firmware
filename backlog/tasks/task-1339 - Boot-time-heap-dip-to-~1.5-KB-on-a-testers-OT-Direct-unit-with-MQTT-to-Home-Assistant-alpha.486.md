---
id: TASK-1339
title: >-
  Boot-time heap dip to ~1.5 KB on a tester's OT-Direct unit with MQTT to Home
  Assistant (alpha.486)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T07:29:00Z'
updated_date: '2026-10-25T11:48:00Z'
labels:
  - heap
  - mqtt
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 462000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D capture 2026-10-24 06:41 UTC (alpha.486, transcript-20261024-093559-...-otgw-1020BA16C6BC): banner at uptime 0 min shows minFree 1524 (low/warn/crit 2/0/0); the MQTT stats topic min_free_heap showed 608 (may be a retained value from an earlier boot). During the later telnet session the heap stayed >= 59792 B, so the dip is not the TASK-1333 telnet burst. His setup: OT-Direct, OT bus offline (no boiler), SAT enabled, BLE off, weather off, MQTT on to homeassistant.local (mDNS, real HA broker), NTP on. Bench (same firmware, local mosquitto on a wired PC): minFree stays ~22-34 KB. Hypothesis (not verified): the MQTT connect/discovery burst after boot queues many publishes while a slower WiFi broker ACKs late, holding send buffers (same mechanism as TASK-1333, but on MQTT/espMqttClient's outbox); other candidates: mDNS resolution of homeassistant.local, retained messages delivered on subscribe, OT-Direct with the bus offline.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The allocation that drives the boot-time dip is named with a bench reproduction (e.g. throttled or remote broker via the MikroTik router, mDNS broker name) or recorded as not reproducible with what was tried
- [ ] #2 If a fix follows: min_free_heap after boot before/after on the same reproduction
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 consolidated open questions re-posted to Sergeant D in #alpha-testing (Robert asked to make the wait explicit); waiting for his answer.

2026-10-24 bench (OTGW32 alpha.486, OT-Direct, no boiler, SAT on, BLE off, local mosquitto; reboot, read hd_min_free_heap after ~90 s, no telnet): broker by IP: 23036, 26036, 21280 B; broker by IP with the router throttling the unit to 64 kbit/s: 24572 B (slow-broker hypothesis not supported); broker by mDNS name Laptop-Robert.local: 14188, 25732, 12772 B. mDNS runs are lower in 2 of 3, so name resolution may add a ~9-11 KB transient, but the spread is too large to call it; none reached his 1.5 KB. Asked Sergeant D (question 5) for entity count, broker location and one capture with the broker set by IP.

2026-10-24 Sergeant D (https://discord.com/channels/812969634638725140/1514720723980259460/1563464620956450840, answers 5a-5c): 405 entities in the MQTT integration; broker is the Mosquitto add-on on the HA machine; he will send a capture with the broker set by IP. 405 entities is far more than the bench publishes: the discovery burst size is the next suspect to reproduce.

2026-10-25 bench (wf_41c083ef, refuted by a second reader; OTGW32 alpha.504+51c1290, OT-Direct, SAT on, no boiler, broker 192.168.88.32 by IP, reboot via /ReBoot, read after ~90 s, no telnet): boot hd_min_free_heap 14788, 3360 (max block 1076, enter low/warn 3/1), 12504, 8448, 4596 B (n=5, median 8448). So a near-repro of Sergeant D's 1524 B is possible on the bench, with a wide spread. Foreign HA discovery tree ruled out by code AND broker: the unit subscribes only to OTGW/set/<node>/#, homeassistant/status and 6 v1 topics for 5 s (MQTTstuff.ino:1364-1374, :1577-1620); discovery-verify filter is <prefix>/+/<node>/# and starts only at uptime >= 3600 s with NTP (mqtt_discovery_verify.cpp:109-117); receive path uses fixed buffers (MqttClient.h:132 RX 1440, Parser.h:32, MQTTstuff.ino:795/:1076); outbound publishes are malloc'd per packet (Packet.cpp:323-324, EMC_USE_MEMPOOL 0). Broker .32 delivers 0 retained messages to the unit at boot (checked read-only). The 400-config preload was not run (blocked by the auto-mode classifier; the code says it cannot matter). Not an A/B with the 2026-10-24 local-mosquitto runs (other firmware, session, weather on, RSSI -74). Side finding (code): getHeapHealth() counts tier entries only when polled (helperStuff.ino:922-931), so short dips below 5120/3072 leave enter_warning/critical at 0 (seen: 4596 B with enter_warning 0); hd_min_max_block is a 1 Hz sample (helperStuff.ino:422-424). Next: telnet [HEAP] trace across the connect steps (MQTTstuff.ino:1340-1385) over a few boots, to name the step that holds the dip.

2026-10-25 telnet heap trace, 5 boots (wf_7af6a40c, corrected by a second reader; alpha.504, logs = HEAD src). Boot watermarks 9772 / 1436 (enter_critical 1, block 1652) / 1520 / 6828 / 480 (block 1332) B. MQTT connect queueing costs 4.4-10.4 KB ([HEAP] MQTTstuff.ino:1171-1385), but the real drop comes when the queued packets go out: boot 3 free 68164 -> 23400 within 94 ms after post-versioninfo, watermark 87684 -> 13496 before the first drip tick. Deep steps line up with MQTT publish bursts on a poor link (RSSI -74): the connect burst (boots 3, 4), large drip ticks (boot 4, OT ID 252 = 35 topics -> 6828), and the backlog after a 2 s link stall (boot 3 -> 1520 at ~83 s, discovery done). Candidate allocation: espMqttClient Outbox, one malloc per packet (Packet.cpp:315-324, EMC_USE_MEMPOOL 0, Outbox.h:91-103); Helpers.h:18 checks max(getMaxAllocHeap, getMaxAllocPsram) so on a PSRAM unit the 16384 B guard never refuses (bench has no PSRAM; Sergeant D's PSRAM unknown). Status: correlation, not proof. Caveat: hd_min_free_heap sums per-region minima (esp_heap_caps.h:222-227), a worst-case lower bound. Next measurement (needs a small diagnostic code change, maintainer's call): log outbox depth/bytes and use heap_caps_monitor_local_minimum_free_size_start (esp_heap_caps.h:258) as a windowed watermark around the connect burst and drip ticks; ask Sergeant D for PSRAM status and RSSI.

2026-10-25 Robert OK: add the diagnostic measurement (telnet log lines only, no behaviour change); ask Sergeant D for PSRAM + RSSI (asked, msg 1563881853310476390). Plan: platform shims for heap_caps_monitor_local_minimum_free_size_start/_stop (esp_heap_caps.h:258) in platform_esp32.h; a windowed watermark + mqttClient.queueSize() (public API, MqttClient.h:70) logged as [HEAP] lines around the onMqttConnect burst (until the outbox drains) and around each discovery drip tick; no change in src/libraries.
<!-- SECTION:NOTES:END -->
