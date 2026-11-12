---
id: TASK-1746
title: >-
  Unconfirmed crash risk: vendored AsyncTCP accept callback returns ERR_OK after
  tcp_abort() when the AsyncClient allocation fails
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T06:21:00Z'
updated_date: '2026-11-12T15:35:00Z'
labels:
  - asynctcp
  - crash-risk
  - library
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 850000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1675 review (system32-78, 2026-11-09, read-only; not observed on a device). AsyncTCP_detail::tcp_accept() in the ESP32Async AsyncTCP library (.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp:1545-1585): when 'new AsyncClient' fails (log '_accept failed: couldn't allocate client', ~1580) or there is no onConnect callback (~1582), it calls tcp_abort(pcb) (~1583) and then returns ERR_OK (~1584). The 'couldn't complete setup' branch just above (~1572-1578) returns ERR_ABRT correctly. lwIP contract: lwip/tcp.h (ESP32-S3 framework libs, lwIP 2.2, D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/include/lwip/lwip/src/include/lwip/tcp.h:61-69, tcp_accept_fn): 'Only return ERR_ABRT if you have called tcp_abort from within the callback function!', i.e. after tcp_abort the callback must return ERR_ABRT. tcp_in.c handles the accept event as 'TCP_EVENT_ACCEPT(pcb, ERR_OK, err); if (err != ERR_OK) { if (err != ERR_ABRT) tcp_abort(pcb); return ERR_ABRT; }' and otherwise goes on using pcb (cited from the lwIP 1.4 copy in framework-arduinoespressif8266@3.20704.0/tools/sdk/lwip/src/core/tcp_in.c:755-763; the ESP32 lwIP 2.2 ships precompiled, its tcp_in.c is not on this machine, so the 2.2 code path is NOT verified here). With ERR_OK after an abort, lwIP would continue on a freed pcb: a use-after-free that can crash under heap exhaustion (an AsyncClient allocation failing at the accept of a new web/telnet connection). Not linked to the TASK-1675 hang. A fix is a one-line library change (return ERR_ABRT after tcp_abort) in a vendored/pinned library, which needs Robert's OK; check upstream ESP32Async/AsyncTCP for an existing fix first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decides: patch the pinned library, take an upstream release that fixes it, or accept the risk
- [ ] #2 Upstream ESP32Async/AsyncTCP checked for this path (issue/commit link or 'not fixed upstream')
- [ ] #3 If patched: a host or device test that forces the AsyncClient allocation to fail at accept shows no crash, red on the old code where reproducible
- [x] #4 Firmware closes the port-80 listener before the /ReBoot and the reset-wireless teardown (no new accept during teardown); host or source test on the real reboot paths, red on the old code
- [ ] #5 Bench by the bench-tester on .61: a reboot loop with a poller hammering /api/v2/device/info during /ReBoot, at least 20 reboots on the old build and on the new one; the new build shows no tcp_accept panic
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
First observed instance (relayed by the coordinator, 2026-11-12): 09's bench on .61, beta.215+242633e, TASK-1757 probe run, boot 5 of 12 (W, two Chrome tabs plus Robert's Firefox tab open). PANIC in tiT during the /ReBoot teardown of the old session; backtrace AsyncTCP_detail::tcp_accept <- tcp_input <- ip4_input <- tcpip_thread; free heap 64,736 B. ba's code reading: the beta.215 probe runs only on the loop task, not on this path (a reading, not proof). First reading to check: an accept arriving while the server is being torn down; an allocation failure is unlikely at 64 KB free. Title and label unchanged; Robert decides.

DECISION Robert (2026-11-12, DM via the coordinator, msg 1570428435195502645, verbatim 'B'): firmware-only, no AsyncTCP library change. Close the port-80 listener (AsyncWebServer end / stop accepting) before the /ReBoot (and reset-wireless) teardown, so no accept arrives during the teardown. Evidence that drives it: 09's beta.215 panic (tcp_accept during teardown, 1 in 12 reboots). AC1 ticked with this answer; AC2 (upstream check) and AC3 (library patch test) belong to the library options Robert did not choose and are superseded by the new AC4/AC5.

Claimed by otgw-firmware-95 (2026-11-12). Starts after beta.217 (TASK-1865 B+C) is built and posted; number beta.219 (ba has 218). Consider TASK-1868 (reboot label) in the same change: it touches the same doRedirect path in FSexplorer.ino:795.

Implementation (95, 2026-11-12, local commit on lane-95-1746, together with TASK-1868; local bump beta.218, becomes beta.219 after ba's 218 is posted): prepareForReboot() (helperStuff.ino) now calls server.end() as its first step, before the MQTT disconnect, the WS close, the 25238 stop and doRestart()'s delay(2000). Placed in the shared teardown, so it covers every planned reboot path (/ReBoot, reset wireless, OTA, nightly, WiFi supervisor), not only /ReBoot.

Coordinator's caution checked (server.end() must not abort the in-flight /ReBoot redirect): pinned AsyncTCP 3.4.10 (.pio/libdeps/esp32-combo/AsyncTCP/src/AsyncTCP.cpp:1532-1542): AsyncServer::end() does tcp_arg(_pcb,NULL), tcp_accept(_pcb,NULL), tcp_close(_pcb) (tcp_abort on failure) on the LISTEN pcb only, under tcp_core_guard; it keeps no client list and touches no client. AsyncWebServer::end() (ESPAsyncWebServer 3.11.2, WebServer.cpp:116) only calls _server.end(). An accept that lwIP queued before end() still dispatches to the (global, still alive) server's connect callback (AsyncTCP.cpp:310-312, 1588) and is served normally. So open connections and their queued responses stay; only new handshakes stop.

Evidence (host, real functions): tests/test_reboot_listener_and_reason.py compiles the real prepareForReboot(), doRedirect(), reBootESP() and resetWirelessButton() with stubs. Old code (lane-95w 84c351493): 3 FAIL / 2 OK (teardown order 'mqtt,ws,otgwstream', no server.end; reasons 'Reboot after upgrade'). Fix: 5/5 OK (order 'server.end,mqtt,ws,otgwstream'; redirect page still built and restFinalize before requestDeferredReboot; AsyncServer::end touches only _pcb). tests/test_fsexplorer_relative_urls.py regex updated for the reason argument, OK. 12 tests slicing FSexplorer.ino/helperStuff.ino all exit 0; evaluate --quick 0 failed. AC4 ticked on this evidence. AC5 (bench: 20+ reboots old vs new with a device/info poller, plus the /ReBoot redirect page still arriving) open for 09.
<!-- SECTION:NOTES:END -->
