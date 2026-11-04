---
id: TASK-1300
title: >-
  WebSocket text frame failed UTF-8 decoding once, closing the browser's /ws
  connection (alpha.454, gruijter)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T12:01:00Z'
updated_date: '2026-10-23T15:35:00Z'
labels:
  - websocket
  - webui
  - bug
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 425000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
browser.log 2026-10-22 11:19:05.567: 'WebSocket connection to ws://10.0.40.73/ws failed: Could not decode a text frame as UTF-8' (one occurrence in 1 h 44). A WS text frame carried a byte sequence that is not valid UTF-8, so Chrome closed the socket; the page reconnects, but the live log/graph lost the frames in between. Find which sender can emit non-UTF-8 bytes on the WS text path (PIC byte stream verbatim per ADR-177 into the OT log line? a Dallas label? event_report?) and either send such frames as binary or sanitise at the WS boundary without touching the verbatim PIC path. Reproduce with a replay line containing a 0x80-0xFF byte.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The byte source is identified with evidence (replay or capture) and recorded
- [x] #2 A replay with a non-UTF-8 byte no longer closes the browser WS connection (bench, browser console)
- [x] #3 ADR-177 (verbatim PIC stream) stays intact on the serial/log path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Fix at the single WS text funnel sendLogToWebSocket (webSocketStuff.ino:279): pure header WsUtf8.h with wsUtf8Valid(const char*) (strict: no overlong C0/C1, no F5+, no surrogates ED A0-BF, correct continuation counts, like the WHATWG decoder Chrome uses) and wsUtf8Sanitize(in, out, outSize) (valid sequences copied, each invalid byte -> '?', an incomplete trailing sequence dropped; output never longer than input). Fast path: valid -> send msg unchanged (no copy). Slow path: sanitize into a static 512-byte buffer (same size as ot_log_buffer; static instead of stack: async_tcp/loop stack budget), send that; a counter of sanitised frames for diagnostics is optional (skip, YAGNI). Raw path, telnet, 25238 and MQTT untouched (ADR-177 verbatim). Tests: tests/test_ws_utf8.cpp on the real header (ascii, '°', lone 0xb0, 0xe9 + '.', trailing 0xc2, overlong C0 80, surrogate ED A0 80, 4-byte F0 9F 98 80 valid, F5, truncation case) red on a naive passthrough / green on the fix; bench: flash app only (settings kept), rerun the agent's wsrun.py (zero invalid frames) and chromews.mjs (no 'Could not decode', no close) with fix_all.log; old firmware run already recorded (red). build.bat combo, evaluate.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 cycle: skipped for now, needs a firmware build and the PC is low on memory (a build was stopped by Claude Code); resume after the maintainer's go.

2026-10-23 17:10 source identified (bench .88.61 alpha.464, agent repro, evidence scratchpad/t1300): every WS text frame goes through sendLogToWebSocket (webSocketStuff.ino:279) with no UTF-8 check. Sources: (a) raw PIC bytes on non-raw paths: unparsed-line events reportOTGWEvent(buf,'<') OTGW-Core.ino:5485/5578/5588, the OT frame decode line :5400/:5465 (isvalidotmsg only checks length/first char), PS=1 fields :4457, echoed commands :3942/:5693/OTDirect.ino:625; (b) AddLog/AddLogf truncation at 511 bytes cuts a valid multi-byte sequence (x*N + '°' -> frame ends 0xC2). Repro: raw client strict-decode errors for 'T80190000\xb0' (0xb0 pos 27), 'PR: E=4\xe9.0', lone '\xc2', 'T8019000\xb0' via the decode line, and the 511-byte truncation; headless Chrome on the real page: 'WebSocket connection ... failed: Could not decode a text frame as UTF-8', closed, reconnect 5 s later, every replay; control fixture: no error. Diagnose raw path (forwardDiagnoseChunk :495-517) is already ASCII-filtered and stays untouched (ADR-177).

2026-10-23 17:35 fix evidence: WsUtf8.h (strict WHATWG ranges) + sendLogToWebSocket sanitises a copy only when the line is invalid (fast path unchanged); wsSafeBuf is a named global (ADR-053, moved out of a function static) with an ADR-090 in-use flag (drop on contention; reviewer found the async REST path can reach the funnel, though it only sends ASCII constants). Host test tests/test_ws_utf8.cpp: fix 0 failures (23 PASS incl. a sweep of all single bytes and outSize limits), naive passthrough 11 failures. Reviewer exhaustive check: 406,900 strings vs Python strict decoder, zero mismatches. Bench .88.61 (app-only flash, settings kept): raw client with fix_all.log: 'text frames 106/107 invalid 0' (old firmware: every bad variant invalid); headless Chrome on the real page: no 'Could not decode', no close (old: closed on every replay); telnet still carries the raw byte 'T80190000\xb0' verbatim (ADR-177, AC3). evaluate 0 fail; build.bat SUCCESS (17:32). Pre-existing ot_log_buffer race filed as TASK-1321.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Chrome closed the live WebSocket when a text frame carried bytes that are not valid UTF-8: raw PIC bytes on the decoded log/event paths, or a log line cut at 511 bytes inside a multi-byte character. sendLogToWebSocket (the single WS text funnel) now checks the line and, only when invalid, sends a sanitised copy (invalid bytes -> '?', a cut trailing sequence dropped). Serial, telnet, port 25238 and the raw diagnose path keep the bytes verbatim (ADR-177). Evidence: AC1 bench reproduction with replayed bad lines (raw client decode errors, Chrome close); AC2 same replay on the fix: 0 invalid frames, no Chrome error; AC3 telnet shows the raw 0xB0 byte. Host test 0 failures vs 11 on a passthrough. Not verified: the original field trigger line (only reproduced by construction).
<!-- SECTION:FINAL_SUMMARY:END -->
