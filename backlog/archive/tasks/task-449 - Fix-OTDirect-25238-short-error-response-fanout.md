---
id: TASK-449
title: Fix OTDirect 25238 short error response fanout
status: Done
assignee:
  - '@codex'
created_date: '2026-04-27 18:03'
updated_date: '2026-04-27 19:07'
labels:
  - otdirect esp32 port25238 codex
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - src/OTGW-firmware/OTGW-Core.ino
  - evaluate.py
  - tests/test_evaluate.py
documentation:
  - docs/c4/c4-component-opentherm-core.md
  - docs/c4/c4-code-otdirect.md
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support. Codex audit found that OTDirect normal synthesized replies are written to TCP port 25238 before processOT, but direct rejection paths still call processOT with NG, BV, OR, SE, NS, or NF without writing the PIC-style response line to OTGWstream. That means an ESP32 OTGW32 client connected to port 25238 can send a bad command and receive no short rejection reply, unlike the PIC-backed bridge where PIC output is fanned out by dispatchOTGWInputLine. Implement a minimal OTDirect helper that writes the two-letter status to OTGWstream with CRLF and then preserves the existing processOT(code, 2) parser side effects.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTDirect rejection paths for NG, BV, OR, SE, NS, and NF write the same two-letter code plus CRLF to OTGWstream before preserving existing processOT(code, 2) behavior.
- [x] #2 Normal OTDirect synthesized responses and raw OpenTherm frame fanout remain unchanged; PIC-backed HAS_PIC behavior remains unchanged.
- [x] #3 Static evaluator coverage fails if OTDirect short rejection responses bypass the 25238 bridge helper.
- [x] #4 Verification is run with the smallest relevant test command and no direct PlatformIO invocation.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add a tiny OTDirect helper beside otDirectBridgeWriteLine that writes a non-empty short status line to OTGWstream with CRLF and then calls processOT(code, len). 2. Replace only the direct handleOTDirectCommand rejection calls for NG, BV, OR, SE, NS, and NF with the helper, leaving normal synthesizeResponse and bridgeFrameToParser output unchanged. 3. Extend evaluate.py and tests/test_evaluate.py so a direct short-error processOT call in OTDirect fails the OTDirect 25238 bridge audit. 4. Run the focused evaluator tests without invoking PlatformIO directly; update task notes, AC, final summary, and status based on results.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented OTDirect short status bridge fanout: added otDirectBridgeProcessStatus(status) beside otDirectBridgeWriteLine(), converted direct NG/BV/OR/SE/NS/NF rejection paths in handleOTDirectCommand() to use it, and preserved processOT(status, 2) parser side effects. Added evaluator key short_error_fanout and a negative unit test that models the previous direct processOT("NG", 2) bypass. Verification run: .\.venv\Scripts\python.exe tests\test_evaluate.py passed, 35 tests OK. Build was not run because this plan explicitly used the smallest non-build verification; per docs/guides/pr-checklist.md the task is not moved to Done until build/checklist verification is complete.

Completion verification on 2026-04-27: tests/test_evaluate.py passed with 35 tests OK; tests/test_build.py passed with 9 tests OK; full .\.venv\Scripts\python.exe build.py passed on rerun for ESP8266 and ESP32-S3, producing firmware, LittleFS, merged binaries, and distribution zips under build/. First full build attempt failed in ESP8266 with a transient .pio build artifact error (missing .pio\build\esp8266\ld\local.eagle.app.v6.common.ld) and left a stale pio process; rerun completed successfully. evaluate.py --quick was run with --verbose: OTDirect 25238 bridge audit passed, including short status fanout; overall evaluator still exits non-zero because of the existing global PROGMEM compliance failure (15 violations) plus two warnings unrelated to this TASK-449 change. version.h and data/version.hash are modified by build.py as expected for this repo build flow and remain intentionally unreset.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented and verified the OTDirect 25238 short status response fanout for ESP32 OTGW32 no-PIC mode. Added otDirectBridgeProcessStatus(status), which writes the two-letter PIC-style status line to OTGWstream with CRLF and then calls processOT(status, 2), preserving existing parser, MQTT, WebSocket, and command queue side effects. Replaced the direct handleOTDirectCommand rejection calls for NG, BV, OR, SE, NS, and NF so clients on port 25238 now receive the rejection response instead of only seeing the inbound command log. Extended the OTDirect 25238 bridge evaluator with a short_error_fanout guard and added tests proving the old direct processOT("NG", 2) pattern is flagged. Verification: .\.venv\Scripts\python.exe tests\test_evaluate.py passed with 35 tests; .\.venv\Scripts\python.exe tests\test_build.py passed with 9 tests; full .\.venv\Scripts\python.exe build.py passed for ESP8266 and ESP32-S3 on rerun and generated firmware/filesystem/merged/zip artifacts. evaluate.py --quick was run with --verbose: the OTDirect 25238 bridge audit passed, while the overall evaluator still reports the existing global PROGMEM compliance failure and two warnings unrelated to this task.
<!-- SECTION:FINAL_SUMMARY:END -->
