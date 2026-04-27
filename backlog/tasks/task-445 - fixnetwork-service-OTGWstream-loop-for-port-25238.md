---
id: TASK-445
title: fixnetwork-service-OTGWstream-loop-for-port-25238
status: Done
assignee:
  - '@codex'
created_date: '2026-04-27 10:27'
updated_date: '2026-04-27 17:46'
labels:
  - bug
  - esp32
  - otgw32
  - ser2net
  - simpletelnet
  - codex
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-Core.h:20'
  - 'src/OTGW-firmware/OTGW-Core.ino:4566'
  - 'src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:81'
  - 'src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:365'
documentation:
  - 'docs/manuals/en/ch09-api-reference.md:1138'
  - 'docs/adr/ADR-010-multiple-concurrent-network-services.md:173'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support
Coding agent tag: codex

Problem found during the port 23 / port 25238 network audit: the OTGW TCP serial bridge on port 25238 is implemented with `SimpleTelnet<2> OTGWstream`, but the firmware starts the server without servicing its cooperative loop. `startPICStream()` calls `OTGWstream.begin(false)` in `src/OTGW-firmware/OTGW-Core.ino`, but there is no matching `OTGWstream.loop()` call in `doBackgroundTasks()` or `handlePICSerial()`.

Evidence:
- `src/OTGW-firmware/OTGW-Core.h:20-21` declares `OTGW_SERIAL_PORT 25238` and `SimpleTelnet<2> OTGWstream(OTGW_SERIAL_PORT)`.
- `src/OTGW-firmware/OTGW-Core.ino:4565-4567` starts the stream with `OTGWstream.begin(false)`.
- `src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:81-85` shows that `SimpleTelnet::loop()` is responsible for `_acceptNewClients()` and keepalive processing.
- `src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:365-379` shows `_acceptNewClients()` is the only path that calls `_server.hasClient()` / `_server.accept()`.
- `rg "OTGWstream\.loop" src/OTGW-firmware` currently returns no firmware call site.

Why this matters:
After the SimpleTelnet migration, `begin()` only binds/listens. It does not accept clients by itself. Port 25238 can appear configured but clients cannot reliably connect or stay connected unless `OTGWstream.loop()` runs from the background task loop. This is the same class of bug as the debug telnet port 23 issue, but it affects the OTmonitor/ser2net-compatible serial bridge.

Implementation guidance:
Keep the fix surgical. Do not replace SimpleTelnet and do not introduce a second server class. Add one clearly named service point for the 25238 stream loop, or call `OTGWstream.loop()` from an existing network-service path where it runs while `isNetworkUp()` is true and flashing is not active. Keep it independent of `HAS_PIC`, because ESP32/OTGW32 also starts `startPICStream()` even though there is no PIC.

Do not call PlatformIO directly for verification; use `build.py`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Port 25238 SimpleTelnet server is serviced by calling `OTGWstream.loop()` from the normal background-service path while the network is up.
- [x] #2 `OTGWstream.loop()` is not hidden behind `#if HAS_PIC`; ESP32/OTGW32 builds must execute the accept/keepalive path too.
- [x] #3 Existing debug telnet port 23 behavior remains unchanged; `debugTelnet.loop()` and `OTGWstream.loop()` are both serviced without sharing mutable line buffers.
- [x] #4 The change uses the existing `SimpleTelnet<2> OTGWstream` instance and does not add another TCP server or heap-heavy abstraction.
- [x] #5 Verification is performed with `build.py --target esp32`; if the wrapper fails after a successful compile due to local artifact locking, record the exact compile success/failure boundary in task notes.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added OTGWstream service helper and wired it into the normal network background path so port 25238 gets cooperative accept/keepalive processing alongside debug telnet.

Verification: tests/test_evaluate.py passed with 34 tests. build.py --target esp32 completed successfully after stale PlatformIO/compiler processes from the previous failed run were stopped; firmware, filesystem, merged binaries, and distribution zip were produced.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented the cooperative SimpleTelnet service path for the OTGW TCP serial bridge on port 25238.

Changes:
- Added handleOTGWstream() as the dedicated OTGWstream.loop() service helper.
- Wired handleOTGWstream() into the normal network background path while the network is up.
- Kept the existing SimpleTelnet<2> OTGWstream instance; no new TCP server or heap-heavy abstraction was introduced.
- Kept debug telnet service independent via handleDebug().

Verification:
- .\\.venv\\Scripts\\python.exe tests\\test_evaluate.py: 34 tests passed.
- .\\.venv\\Scripts\\python.exe build.py --target esp32: completed successfully, including firmware, LittleFS, merged binaries, and distribution zip.
<!-- SECTION:FINAL_SUMMARY:END -->
