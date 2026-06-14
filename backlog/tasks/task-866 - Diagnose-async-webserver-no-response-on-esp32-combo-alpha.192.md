---
id: TASK-866
title: Diagnose async webserver no-response on esp32-combo (alpha.192)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-14 20:30'
updated_date: '2026-06-14 20:41'
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
<!-- SECTION:NOTES:END -->
