---
id: TASK-454
title: Fix OTDirect 25238 PR response fanout
status: Done
assignee:
  - '@codex'
created_date: '2026-04-27 19:37'
updated_date: '2026-04-27 20:23'
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
Branch: feature-dev-2.0.0-otgw32-esp32-sat-support. Hardware test on 2026-04-27 showed that PR=A over TCP port 25238 is processed by OTDirect and visible on debug telnet port 23 as PR: A=OpenTherm Gateway OTGW32, but the response is not returned to the 25238 client. Root cause: the OTDirect PR= query branch synthesizes prBuf lines and calls processOT(prBuf, strlen(prBuf)) directly, bypassing otDirectBridgeWriteLine(). The PIC-backed path fans PIC output to OTGWstream before processOT via dispatchOTGWInputLine, so OTDirect must mirror that for PR responses.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All OTDirect PR= query response paths write the generated PR response line to OTGWstream with CRLF before calling processOT().
- [x] #2 PR=A over port 25238 returns the synthesized PR response to the TCP client instead of only logging it on port 23.
- [x] #3 Existing PR response parser side effects are preserved by continuing to call processOT() with the same generated buffer and length.
- [x] #4 Static regression coverage fails if OTDirect PR= responses call processOT(prBuf, ...) without using the 25238 bridge fanout helper.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add a tiny PR response helper next to synthesizeResponse/bridge helpers that writes the generated PR line to OTGWstream via otDirectBridgeWriteLine() and then calls processOT() with the same buffer and length. 2. Replace only the PR= branch direct processOT(prBuf, strlen(prBuf)) calls with this helper. 3. Extend the OTDirect 25238 evaluator/test coverage to detect direct PR processOT bypasses. 4. Run focused unit tests and leave full build/hardware smoke status documented in the task.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented PR response fanout for OTDirect 25238: added otDirectBridgeProcessPRResponse(prLine), which writes the generated PR line to OTGWstream with CRLF and then calls processOT(prLine, prLen). Converted all PR= query response paths from direct processOT(prBuf, strlen(prBuf)) to the helper. Added evaluator key pr_response_fanout and a negative test for the old direct processOT(prBuf, strlen(prBuf)) bypass. Verification: .\.venv\Scripts\python.exe tests\test_evaluate.py passed with 36 tests OK; .\.venv\Scripts\python.exe build.py --target esp32 passed and produced ESP32 firmware/filesystem/merged/zip artifacts. AC #2 remains unchecked until the flashed OTGW32 is retested on TCP port 25238 with PR=A.

Hardware validation evidence from 2026-04-27 on branch feature-dev-2.0.0-otgw32-esp32-sat-support: TCP port 25238 log now shows PR=A immediately followed by PR: A=OpenTherm Gateway OTGW32, proving the synthesized PR response is fanned out to the 25238 client. Same log shows PS=1/PS=2/PS=0 returning PS responses, unknown ZZ=1 returning NG, and unsupported/invalid MI=99 returning OR while OT frames continue as R80000100/R10012D00.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed OTDirect 25238 PR response fanout for ESP32/OTGW32 mode. PR= query responses generated inside OTDirect now go through the same 25238 bridge fanout path before processOT(), matching the PIC-backed dispatch behavior while preserving existing parser side effects. Added evaluator regression coverage so direct processOT(prBuf, ...) bypasses in the PR response path are flagged. Verification completed with tests/test_evaluate.py passing, build.py --target esp32 passing, and hardware validation on 2026-04-27 showing PR=A over TCP port 25238 returns PR: A=OpenTherm Gateway OTGW32 to the 25238 client.
<!-- SECTION:FINAL_SUMMARY:END -->
