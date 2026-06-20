---
id: TASK-866
title: Diagnose async webserver no-response on esp32-combo (alpha.192)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-14 20:30'
updated_date: '2026-06-20 10:55'
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

INSTRUMENTATION LANDED (commit bc84b9b7, 2.0.0-alpha.200). Loop-stall detector added to pin the >=30s blocking section without a USB cable: (1) HeapDiagSection.iMaxLoopGapMs = longest gap between loop() entries since boot; (2) loop() logs '[loop-stall] N ms gap' for any gap >200ms (the telnet timestamp brackets the blocking section against the preceding per-section debug line); (3) banner 'Drops:' line shows 'maxLoopGap N ms' and the 'D' state dump shows 'max_loop_gap_ms'. Diagnostic-only, zero behaviour change when healthy. Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick 98.6%. FIELD STEP to close AC#1: flash alpha.200 to the OTGW32, let it run, then read the banner 'maxLoopGap' + grep the telnet log for '[loop-stall]' lines. The largest gap's timestamp, matched against the line just before it (processOT / loopOTDirect / an NTP/DNS/WiFi/LittleFS section), names the culprit. If the gap is ~30s+ that is the TWDT trigger; if it is 4-8s that is the AsyncTCP-starvation latency floor. Escalate to per-section checkpoint labels (Option B) only if the bracketing is ambiguous.

ROOT-CAUSE HUNT (wf_79e88a89-6f8, full detail in TASK-879 notes). Top suspect for AC#1: synchronous SimpleTelnet/WiFiClient writes on the loop task (port 23 debug + port 25238 ser2net). NetworkClient::write blocks ~10s/write on a stalled/half-open client (1s select x 10 retries, no feedWatchDog); every Debug* macro + every OT-frame mirror funnels through it on the loop task; debug-all-on + alpha auto-enable makes the stream dense. OBSERVER-COUPLED: the stall exists only while a telnet/ser2net client is connected (SimpleTelnet::write early-returns at _connectedCount==0), and our capture is taken over telnet -> the AC#2 'dispatch alive but slow' is the AsyncTCP task being starved by the loop-task socket-write block, NOT a webserver-handler fault. a-priori suspects MQTT-broker-DNS + NTP-DNS ELIMINATED by code (MQTT disabled path returns before hostByName; NTP configTime is async). KILL-TEST to close AC#1/#2: power-cycle, connect NOTHING to 23/25238, curl http://<ip>/ -> floor vanishes confirms; persists -> reopen compute/LittleFS sweep. Fix touches vendored SimpleTelnet (read-only per policy) -> maintainer decision (availableForWrite gate + drop, or FreeRTOS ring-buffer offload).

OBSOLETE (audit wp0vjoo5s): diagnostic stub. Its 'core-1 starvation' conclusion was MECHANISTICALLY OVERTURNED by TASK-879 (async_tcp prio10>loopTask prio1 preempts; real cause = lock-contention + telnet RX-timeout + LittleFS-FD/heap). The fix shipped under TASK-879 (AsyncSimpleTelnet a125fbf7, bounded OTStateLock, ADR-147 static-file gate 5a92b26e) and is live on HEAD. The combo browser re-capture field gate lives on TASK-879 (In Review).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as resolved-by-successor TASK-879/ADR-147. Root cause + fix landed under TASK-879; keeping one live task (879) for the OTGW32 field re-capture instead of two.
<!-- SECTION:FINAL_SUMMARY:END -->
