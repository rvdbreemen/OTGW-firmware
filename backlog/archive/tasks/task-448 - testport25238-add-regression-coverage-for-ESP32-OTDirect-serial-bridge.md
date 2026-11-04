---
id: TASK-448
title: testport25238-add-regression-coverage-for-ESP32-OTDirect-serial-bridge
status: Done
assignee:
  - codex
created_date: '2026-04-27 10:29'
updated_date: '2026-04-27 17:47'
labels:
  - test
  - esp32
  - otgw32
  - ser2net
  - otdirect
  - simpletelnet
  - codex
dependencies:
  - TASK-445
  - TASK-446
  - TASK-447
references:
  - 'src/OTGW-firmware/OTGW-Core.h:20'
  - 'src/OTGW-firmware/OTGW-Core.ino:4566'
  - 'src/OTGW-firmware/OTGW-Core.ino:4291'
  - 'src/OTGW-firmware/OTDirect.ino:447'
  - 'src/OTGW-firmware/OTDirect.ino:1674'
  - 'src/libraries/SimpleTelnet/src/SimpleTelnet_impl.tpp:81'
documentation:
  - 'docs/manuals/en/ch09-api-reference.md:1138'
  - 'docs/adr/ADR-010-multiple-concurrent-network-services.md:173'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support
Coding agent tag: codex
Depends on: TASK-445, TASK-446, TASK-447

Purpose:
Add regression coverage for the port 25238 TCP serial bridge after fixing the SimpleTelnet service loop and OTGW32/OTDirect no-PIC bridge behavior. This task exists because the failure mode is structural: the code compiled, but the cooperative server loop and ESP32 no-PIC bridge path were not wired in a way that a compiler would catch.

Audit findings to lock down:
- Port 25238 uses `SimpleTelnet<2> OTGWstream`, so it requires `OTGWstream.loop()` just like debug telnet requires `debugTelnet.loop()`.
- ESP32/OTGW32 has `HAS_PIC 0` and `HAS_DIRECT_OT 1`, so the port 25238 bridge cannot live only inside `handlePICSerial()`.
- The no-PIC command bridge should be owned by `OTDirect.ino`, with core/network code only servicing the socket and calling the OTDirect bridge entry point.
- OTDirect must support both directions: commands from TCP clients into `handleOTDirectCommand()`, and OTDirect raw frames / synthesized PIC responses back to TCP clients.

Suggested test strategy:
Use the lightest tests that are realistic for this firmware. If host-compilable tests already exist for architecture checks, add text/AST-style regression checks there. If not, add targeted `evaluate.py` checks with clear names. Avoid heavy harness work unless it is already present in the repo.

Candidate checks:
1. Static check: `OTGWstream.loop()` has a firmware call site outside the SimpleTelnet library.
2. Static check: no-PIC 25238 bridge handler symbol is defined in `OTDirect.ino` under `HAS_DIRECT_OT`.
3. Static check: `doBackgroundTasks()` or the relevant network service path calls the OTDirect bridge handler when `HAS_DIRECT_OT` is enabled.
4. Static check: the OTDirect bridge handler reads from `OTGWstream.available()` / `OTGWstream.read()` and routes completed lines to `sendPICSerial()`, `addCommandToQueue()`, or directly to `handleOTDirectCommand()` through a single existing command path.
5. Static check: OTDirect outbound bridge writes lines to `OTGWstream.write()` with CRLF from the helper used by `bridgeFrameToParser()` and `synthesizeResponse()`.
6. Build check: `build.py --target esp32` compiles after the bridge changes.

Manual smoke test to document in implementation notes if hardware is available:
- Flash or run an ESP32/OTGW32 build.
- Connect with `nc <device-ip> 25238` or `telnet <device-ip> 25238`.
- Send `PR=A` followed by CR.
- Expected: client sees a PIC-compatible response such as an OTGW32 `PR:` response from OTDirect.
- Send a supported write command such as `TT=20.0` followed by CR.
- Expected: client sees a command response or a raw OTDirect frame, and WebSocket/MQTT state handling still flows through `processOT()`.

Do not call PlatformIO directly for verification; use `build.py`.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Regression coverage fails if `OTGWstream.loop()` is removed from firmware service code while `SimpleTelnet<2> OTGWstream` remains the 25238 server.
- [x] #2 Regression coverage fails if the no-PIC ESP32 bridge handler is moved out of `OTDirect.ino` or hidden entirely behind `HAS_PIC`.
- [x] #3 Regression coverage fails if the OTDirect no-PIC bridge handler no longer reads incoming bytes from `OTGWstream` and routes complete commands into the existing PIC-style OTDirect command path.
- [x] #4 Regression coverage fails if OTDirect synthesized responses or raw frame lines stop writing to `OTGWstream` clients after TASK-447 is implemented.
- [x] #5 The tests/checks are documented with names that explain the behavior, not just the implementation detail, so a future agent understands why the guard exists.
- [x] #6 `python evaluate.py --quick` or the relevant targeted test command is documented in the task final summary when implemented.
- [x] #7 `build.py --target esp32` is run during implementation, or the task notes clearly explain why it could not be run in that environment.
- [x] #8 Regression coverage documents and checks the intended ownership split: `OTGWstream.loop()` may be serviced from core/network code, but the no-PIC ESP32 bridge input/output handling is recreated in `OTDirect.ino` from `handlePICSerial()` and `dispatchOTGWInputLine()` behavior.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added evaluate.py OTDirect 25238 bridge regression gate and unit tests. The gate now requires both the OTGWstream.loop() helper and the firmware background call site, so it fails if the helper exists but is no longer serviced. It also checks OTDirect ownership of the no-PIC bridge, inbound OTGWstream read routing, and outbound OTGWstream fanout. Verification passed with tests/test_evaluate.py and build.py --target esp32.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added regression coverage for the ESP32/OTDirect port 25238 bridge.

Changes:
- Added otdirect_25238_bridge_regressions() and check_otdirect_25238_bridge() to evaluate.py.
- Wired the bridge audit into evaluate_all(quick=True), so quick evaluation includes the guard.
- Added unit tests for the current source wiring and negative cases: missing service loop, unserviced helper, missing inbound bridge, missing outbound fanout, and broken ownership split.
- The checks document the intended ownership split: core/network services OTGWstream.loop(); OTDirect.ino owns the no-PIC ESP32 bridge input/output behavior.

Verification:
- .\\.venv\\Scripts\\python.exe tests\\test_evaluate.py: 34 tests passed.
- .\\.venv\\Scripts\\python.exe build.py --target esp32: completed successfully, including firmware, LittleFS, merged binaries, and distribution zip.
<!-- SECTION:FINAL_SUMMARY:END -->
