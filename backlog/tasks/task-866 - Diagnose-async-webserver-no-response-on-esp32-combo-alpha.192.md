---
id: TASK-866
title: Diagnose async webserver no-response on esp32-combo (alpha.192)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-14 20:30'
updated_date: '2026-06-16 06:24'
labels: []
dependencies: []
ordinal: 82000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the esp32-combo build (2.0.0-alpha.192+75bcd07, board mode 2 OT-Direct), the ESPAsyncWebServer never returns an HTTP response. Capture 20260614-214456 proves: firmware healthy (telnet/loop/OTDirect alive, heap HEALTHY), WiFi connected (192.168.88.39), fs:ok (LittleFS + assets present), MQTT disabled (stream is other devices' retained), PIC no-pic + LED 1Hz both expected. Browser GET http://OTGW/ => PENDING no-response (no responseReceived, no loadingFailed) for 67s. Capture-tool semantics (capture-mqtt-debug.bat:1118-1148) prove no HTTP headers and no RST/refused: TCP handshake completes but the AsyncTCP service task never dispatches the request to a route handler. Root in the same-day async migration (TASK-865.9..14) + ADR-123 FreeRTOS topology. AsyncTCP=ESP32Async/AsyncTCP@3.4.10, no task-config overrides. Exact mechanism (task-not-started vs stalled vs starved) needs USB boot-log + curl-by-IP. Plan: plans/federated-leaping-moonbeam.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 USB boot-log captured and HTTP-server/AsyncTCP startup outcome is visible (mechanism pinned: task-not-started vs stalled vs starved)
- [ ] #2 curl -v http://<ip>/ and a trivial route reproduce/characterize the hang (confirms dispatch-layer, not handler-specific)
- [ ] #3 Root mechanism fixed; curl http://<ip>/ returns 200 + index.html and web UI loads in browser
- [ ] #4 Re-capture browser-log shows [net] <ms> 200 instead of PENDING no-response
- [ ] #5 python build.py --target esp32-combo exit 0 and python evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
User primary-source curl: `curl http://otgw.local/api/v2/devinfo` -> {"error":{"status":404,"message":"Endpoint not found"}}. Webserver DOES respond (structured 404 from API router) => dispatch layer is NOT dead. Path was wrong: real endpoint is /api/v2/device/info (not devinfo). Conclusion narrowed: capture-time '/' hang is either (1) chunked index-emitter sendIndex-specific while /api/* works, or (2) transient at fresh boot. Added a post-capture curl-probe phase to scripts/capture-mqtt-debug.bat (Invoke-HttpProbes): runs after telnet/browser/MQTT teardown, hits / + /index.html + /index.js + /api/v2/health + /api/v2/device/info + /api/v2/device/time + /api/v2/flash/status + /api/v2/sat/status with real curl.exe, --max-time bounded (default 10s, -HttpProbeTimeoutSeconds, -SkipHttpProbes), logs status/time/size/body-snippet to curl-probes.log merged into transcript.txt. Resets $LASTEXITCODE=0 so a probe TIMEOUT doesn't leak into the script exit. Verified: PS parse OK; isolated run against blackhole 192.0.2.1 shows bounded TIMEOUT lines + clean exit. Next run pins (1) vs (2).

MECHANISM PINNED (capture 20260616-074035, alpha.199 OTGW32; same bug class as TASK-879 which is the alpha.199 continuation of this task): the hang is STARVED, not task-not-started and not a dead dispatch layer. The AsyncWebServer SERVES every route 200 OK with full payloads (index.html 40KB, index.js 351KB, all /api/v2/*), but each response is 4-8s late and INDEPENDENT of payload size (169B health=5.07s, 43B flash/status=4.48s) and root / exceeds curl's 10s timeout (=browser 'PENDING no-response'). Heap HEALTHY on fresh boot (~92KB free, 45KB maxBlk) so NOT heap-starvation. ADDED dimension: the device is in a Task Watchdog reboot loop (both telnet banners 'Boot : Task watchdog', reboot counter 2->3 during the capture, no Exception/Guru/Backtrace). TWDT subscribes the LOOP TASK ONLY (timeout 30s, panic; OTGW-Core.ino:1239 + comment 1220) and AsyncTCP is pinned to core 1 (CONFIG_ASYNC_TCP_RUNNING_CORE=1) where Arduino loopTask also runs => the loop task blocks core 1 for seconds (worst right after boot: 7.4s@42s uptime -> 3.8s@78s), starving AsyncTCP (slow HTTP) and, when it blocks >=30s without feedWatchDog()->esp_task_wdt_reset(), tripping the TWDT (reboot). ONE root cause for both AC#1 (mechanism=starved) and the reboots. AC#2 partly met: routes characterized, dispatch confirmed alive. NEXT to close AC#1 fully: pin the >=30s blocking loop section via (a) USB-serial TWDT panic backtrace (panic prints to Serial not telnet) or (b) loop-section-timing instrumentation. Full analysis in TASK-879 notes + .wolf/buglog.json bug-130.
<!-- SECTION:NOTES:END -->
