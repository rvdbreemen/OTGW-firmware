---
id: TASK-879
title: >-
  fix(esp32): Task-Watchdog reboot loop + core-1 starvation (slow webserver),
  OTGW32 alpha.199
status: To Do
assignee: []
created_date: '2026-06-16 05:38'
updated_date: '2026-06-16 05:53'
labels: []
dependencies: []
ordinal: 95000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field bug on 2.0.0-alpha.199+7e3da75 (OTGW32, Board Mode=2 OT-Direct, esp32-combo build). Device fully alive: telnet (port 23) responds, OTDirect loop runs, heap HEALTHY (~86-92KB, frag 51%), hardware.mode=OT-Direct correct, no crash. BUT port 80 refuses all connections: capture-mqtt-debug curl probes (root, /index.html, /index.js, /api/v2/health|device/info|device/time|flash/status|sat/status) ALL return curl exit 7 'Failed to connect to port 80: Could not connect to server' (one exit 28 timeout). server.begin() is at OTGW-firmware.ino:375 (after startWiFi@291, setupFSexplorer@374); loop runs so setup() completed past begin(). So begin() ran but no listening socket. Captive portal (WiFiManager AP) works = separate. NOT from Phase 2 (TASK-871/872 only touched discovery JSON) nor #659. Suspect async webserver stack (ADR-132/133, TASK-865.9/10/11): server.begin() not binding (heap/LWIP PCB at boot) OR AsyncTCP task not coming up (CONFIG_ASYNC_TCP_RUNNING_CORE=1, platformio.ini:52; build-flag inheritance caveat for combo at :104). Evidence: scripts/logs/mqtt-diagnostics/20260616-072742 + 20260616-073251 (transcript.txt, HTTP PROBES section).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root-cause confirmed from a boot-time telnet/USB log (does 'HTTP Server started' print? heap + AsyncTCP core at server.begin()?)
- [ ] #2 Port 80 accepts connections + serves /index.html + /api/v2/health on OTGW32/combo after fix
- [ ] #3 Build green 3 targets; evaluate.py --quick clean
- [ ] #4 Field-validated: webserver reachable on a real OTGW32
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
CORRECTION (capture 073251 full probe list): server is NOT dead-on-boot. After a fresh boot /api/v2/health|device/info|device/time|flash/status|sat/status ALL return 200 OK with JSON. The /,index.html,index.js failures in that capture were BOOT-TIMING (health body uptime=0:00, device was rebooting during the first 3 probes, 26s before the API answered). The REAL bug = capture 072742: at 11-min STABLE uptime ALL 8 probes return curl exit 7 (connection refused). So webserver works fresh, stops accepting after ~minutes. Smoking gun = heap fragmentation: dead capture frag 51% maxBlk ~42-45KB vs fresh maxBlk 51KB. Hypothesis: AsyncTCP/LWIP per-connection alloc fails when maxBlk drops below threshold -> accept() refused. Next: capture telnet banner heap (frag/maxBlk) AT the moment the webserver is dead, compare working-vs-dead maxBlk. Investigate AsyncTCP accept/alloc path + the frag source (single-device discovery republish? espMqttClient? WS?). Reframed from 'never listens'.

DECISIVE REFRAME (capture 20260616-074035): root cause is NOT connection-refused/accept-fail and NOT heap. It is a Task Watchdog reboot loop + loop-task blocking core 1.

EVIDENCE:
- BOTH telnet banners read 'Boot : Task watchdog' (r.68 reboot#2 up7min; r.128 reboot#3 up0min). Reboot counter climbs 2->3 DURING the capture (the ~40s telnet-gap 07:40:48-07:41:27 = device rebooting #2->#3). No Exception/Guru/Backtrace anywhere -> NOT a crash.
- Webserver SERVES everything 200 OK with full payloads (curl-probes r.1809-1823): index.html 40KB, index.js 351KB, all /api/v2/* JSON. NOT dead, NOT refused.
- BUT every response 4-8s late, INDEPENDENT of payload size: /api/v2/health 169B=5.07s, /api/v2/flash/status 43B=4.48s, /index.js 351KB=7.97s. Root / TIMEOUT >10s (curl exit 28). browser.log r.1797: '/ PENDING no-response never finished' = exactly the 'webserver does not respond' symptom (browser hits / first).
- Heap HEALTHY on fresh boot: health JSON heap:93964, device/time freeheap:92468 maxfreeblock:45044. So earlier 'heap-frag/AsyncTCP starvation' framing is WRONG - it is CPU starvation not heap. (Frag does grow 47%->61% over 7min but free stays ~80KB.)
- Latency HIGHEST right after boot (7.4s @42s uptime) and DECREASES (3.8s @78s uptime) -> opposite of heap-leak-over-time; points to heavy synchronous early-boot work.

MECHANISM (code-confirmed): TWDT timeout_ms=30000 trigger_panic=true subscribes LOOP TASK ONLY (OTGW-Core.ino:1239 + comment 1220). feedWatchDog()->esp_task_wdt_reset() (OTGW-Core.ino:1287). AsyncTCP on core 1 (CONFIG_ASYNC_TCP_RUNNING_CORE=1, platformio.ini:52); Arduino loopTask also core 1. => loop task blocks core 1 for seconds: (a) AsyncTCP starved -> slow HTTP; (b) >=30s block without feedWatchDog -> TWDT panic -> reboot. ONE root cause, both symptoms.

ALSO CONFIRMED NOT-A-BUG (this capture): boot-detect WORKS (hardware.mode:OT-Direct r.661, Board Mode:2 r.616, pic available:false); MQTT 'disconnected' is settings.mqtt.enabled:false (disabled by config).

NEXT: (1) USB-serial capture during a reboot for the TWDT panic backtrace (panic prints to Serial, not telnet) = exact hung task+PC. (2) instrument loop path with a 'section took N ms' timer logging >500ms to self-identify the blocking section. Prime suspects: blocking NTP/DNS/WiFi settle on early boot, synchronous otMaster.sendRequest() in OTDirect handshake (OTDirect.ino:920/930), or LittleFS/schedule-init.
<!-- SECTION:NOTES:END -->
