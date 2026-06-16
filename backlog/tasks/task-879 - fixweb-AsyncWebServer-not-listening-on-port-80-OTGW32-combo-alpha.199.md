---
id: TASK-879
title: >-
  fix(esp32): Task-Watchdog reboot loop + core-1 starvation (slow webserver),
  OTGW32 alpha.199
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-16 05:38'
updated_date: '2026-06-16 22:10'
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

ROOT-CAUSE HUNT RESULT (workflow wf_79e88a89-6f8: 16 agents, 7 subsystems, 14 candidates, 8 verified, 0 survived strict refutation -> the cause is a non-primitive socket-write blocker the candidate list never examined).

TOP SUSPECT (rank 1, ~0.5 conf, gated on kill-test): synchronous SimpleTelnet/WiFiClient writes on the LOOP TASK (debug port 23 + ser2net port 25238). Every Debug*/DebugT* macro -> debugTelnet.print -> SimpleTelnet::write (src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:223) -> ESP32 core NetworkClient::write. PRIMARY SOURCE (~/.platformio/.../Network/src/NetworkClient.cpp:386-447): select() WIFI_CLIENT_SELECT_TIMEOUT_US=1000000 (1s) x WIFI_CLIENT_MAX_WRITE_RETRY=10 => ~10s/write block on a stalled buffer; partial progress RESETS retry to 10 (line 432) so a slow-drip/half-open client blocks even longer; send() uses MSG_DONTWAIT so SO_SNDTIMEO is moot; NO feedWatchDog inside (vendored core). OT frames also mirrored to OTGWstream port 25238 via same path (OTGW-Core.ino:502-504) from processOT() in drainOTFrameQueue() on the loop task. Debug defaults bOTmsg/bNTP/bSAT/bOTDirect all true (debugStuff.h:48-57) + alpha auto-enables debug (OTGW-firmware.ino:369-370) => dense stream. OBSERVER COUPLING (decisive): SimpleTelnet::write early-returns when _connectedCount==0 (tpp:219) -> the stall exists ONLY while a telnet/ser2net client is connected, and the capture was taken over telnet -> the measurement induces the symptom. Matches field profile exactly: worst at boot (flood saturates TCP buffer over settling WiFi -> 7.4s), eases as it drains (3.8s). RANK 2 = same mechanism, degraded/half-open-client regime: 3-4 x ~10s writes summing within one inter-feedWatchDog window cross the 30s TWDT -> Boot:Task watchdog reboot (conditional, not proven as a single 30s code event).

REFINEMENT (matches capture 074035): the capture had FAILED telnet reconnects (summary: timeout #1, 'invalid argument EndConnect' #2) which leave HALF-OPEN sockets on port 23. That explains why the curl probes were still 4-8s AFTER telnet teardown (debug writes kept blocking on the half-open socket) -> strengthens rank-1/rank-2.

ELIMINATED by code (do not re-chase): MQTT broker DNS WiFi.hostByName (MQTTstuff.ino:1088 unreachable - handleMQTT returns :1053, startMQTT :573 when MQTT disabled); NTP DNS (configTime async networkStuff.ino:663, loopNTP reads cached time); BLE scan (own host task, satBLELoop only throttles publish); Dallas (setWaitForConversion(false), gated off); mDNS (compiled out ESP32); boot OTDirect probe (setup-only ~1s, pre-loop); per-loop OTDirect (fully async sendRequestAsync+isReady). CONDITIONAL secondary (rank 3): weatherFetch() sync http.begin()+GET (SATweather.ino:378/385), setTimeout bounds reads only not DNS/connect -> real >=30s risk IF bWeatherEnable on; but gated off first 5 min + skipped if OT Toutside valid, so cannot be the sub-5-min floor.

KILL-TEST (decisive, hand to field, no code): power-cycle the OTGW32, connect NOTHING to port 23 or 25238 (no telnet, no OTmonitor/ser2net), then curl http://<device-ip>/ a few times from a machine that never opened telnet to it. Floor VANISHES -> rank-1 CONFIRMED (loop-task socket writes). Floor PERSISTS -> compute/LittleFS-bound, reopen loopMQTTDiscovery/do5+15minevent/doTaskEvery*/loopOLED. The kill-test beats any [loop-stall] line because the detector's own DebugTf blocks on the same stalled socket.

FIX (primary, needs decision - touches vendored SimpleTelnet which is READ-ONLY per policy): gate SimpleTelnet::write on availableForWrite() and DROP bytes that don't fit (lossy is fine for a debug stream) instead of the 10x1s select loop; OR move debug emission + the ser2net mirror onto a dedicated FreeRTOS task fed by a ring buffer the loop only enqueues into (mirrors the existing webhook offload pattern). Do NOT just add feedWatchDog() inside the write - masks the reboot, leaves the AsyncTCP latency floor. Full report: workflow wf_79e88a89-6f8 output.
<!-- SECTION:NOTES:END -->
