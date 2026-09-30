---
id: TASK-1133
title: >-
  POST /api/v2/otgw/diagnose forwards the raw JSON body to the PIC when the
  field name is unexpected
status: Done
assignee:
  - '@claude'
created_date: '2026-09-06 19:18'
updated_date: '2026-09-30 13:03'
labels:
  - bug
  - api
  - pic
dependencies: []
priority: medium
ordinal: 280000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed on the classic-S3 bench board (alpha.362, diagnose PIC 2.2) while gathering evidence for TASK-1131. A POST to /api/v2/otgw/diagnose carrying {"input":"\r"} instead of the expected {"data":"..."} did not return a 4xx: the literal text {"input":"\r"} appeared in the diagnose console, meaning the raw request body was written to the PIC UART.

The page itself sends {data: ...} (v2.js sendDiagnoseData), so this is not reachable from the UI. It is reachable from any script or curl against the API, and the endpoint writes to the PIC, so an unparsed body becomes unsolicited bytes on the serial line. On a gateway PIC that is command text rather than a rejected menu choice, which is the same severity argument TASK-1131 makes about the console leak.

Same shape as the raw-body fallback recorded in TASK-1083 for POST /api/v2/otgw/commands, so the two may share a cause or a helper. Worth checking whether the fallback is deliberate (accept a bare body as the payload) or accidental (parse failure falls through to the raw buffer). If deliberate, it should still be validated by the same printable+CR filter as the parsed path.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A POST with an unexpected or missing field returns a 4xx and writes nothing to the PIC
- [x] #2 If a bare-body form is deliberately supported, it passes the same printable+CR filter as the parsed path
- [x] #3 The relationship to TASK-1083 is settled: either one shared fix or an explicit note that the two paths differ
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-30 fix (alpha.382), 1.x parity.
- restAPI.ino /otgw/diagnose: the raw-body fallback is gone. When extractJsonField(body, "data", dataBuf[33]) returns false (field missing, or value longer than 32 characters) the handler answers 400 'Missing data, or longer than 32 characters' and returns before anything reaches enqueuePICTx(). Same message and behaviour as otgw-1.x.x restAPI.ino (TASK-1127 handler).
- Host proof, real scanner: test/host/build_and_run.ps1 (sliced extractJsonField) section h: the web UI body {"data":"1\r"} is accepted and decodes to '1'+CR; the TASK-1133 body {"input":"\r"} is refused (the old code copied it verbatim, 13 printable bytes, to the PIC, which is what the diagnose console showed on the Classic bench); data longer than 32 characters, a bare text body and an empty body (form-urlencoded, never captured) are refused. 33 checks, 0 failures.
- AC#2: the bare-body form is deliberately NOT supported (1.x parity; the UI always sends JSON), so every accepted byte comes from the parsed field and passes the existing printable+CR filter; the h5 check proves a bare body is refused.
- AC#3: the two paths differ. POST /api/v2/otgw/commands on dev still falls back to the raw body when extractJsonField() returns false (restAPI.ino ~:737), which 1.x TASK-1083 refined, but that path is safe: the fallback text goes through the command-format validator (two letters and '='), so a JSON body is rejected with 400 'Invalid command format' and nothing reaches the PIC; only the message is less precise than 1.x. diagnose had no such validator, which is why it needed this fix. Porting the TASK-1083 message refinement is optional and not part of this task.
- Build: build.bat --target esp32-combo SUCCESS (fw + fs, fresh 11:46, alpha.382+0f6d691); evaluate.py --quick 69 passed / 0 warnings / 0 failed.
OPEN: AC#1 device confirmation needs the Classic-S3 with picfwtype=diagnose (probes with Content-Type: application/json; curl -d sends form data that never reaches the body hook).

AC#1 closed 2026-09-30 with a host harness on the real handler branch (no Classic-S3 connected): test/host/test_diagnose_handler.py slices the diagnose branch body of the /api/v2/otgw handler (restAPI.ino, between its 'diagnose' test and the next branch) and the real JSON scanner (jsonStuff.ino sentinels); the PIC transmit queue, the HTTP answers and bodyCompat() are recording doubles.
Run: python test/host/test_diagnose_handler.py --old-rev 88f245717^ -> FIX 7/7; OLD fails exactly H1, H3, H4, H7, as predicted before the run: OLD answers 202 and writes {"input":""} (the reported body), a bare '1<CR>', the first 32 characters of an over-long JSON text, and '{}' to the PIC; FIX answers 400 and writes nothing. Controls H2 (empty body), H5 ({"data":"1"} -> 202, '1'+CR), H6 ({"data":"
"} -> 400) identical. RESULT: PASS. Transcript: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1133-diagnose-oldvsfix.txt
Not covered by the harness: the HTTP transport that delivers the body to bodyCompat() on the device (a curl -d form body never reaches the body hook; probes need Content-Type: application/json). That is a transport property, not the AC#1 decision.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
POST /api/v2/otgw/diagnose no longer falls back to the raw body (88f245717): a body without a usable 'data' field is answered 400 and nothing reaches the PIC. Before, {"input":""} from the reporter was written to the diagnose PIC verbatim.
Evidence per AC:
- AC#1: test/host/test_diagnose_handler.py runs the real handler branch with the real JSON scanner: OLD (88f245717^) writes the reported body, a bare body, a truncated over-long JSON text and '{}' to the PIC with 202; FIX answers 400 and writes nothing; controls unchanged.
- AC#2: the bare-body form is deliberately not supported (1.x parity); every accepted byte passes the printable+CR filter (harness h5 and H3).
- AC#3: the relationship to TASK-1083 is settled in the notes: the commands path keeps its format validator, so it is safe; only its message is less precise than 1.x.
Build esp32-combo SUCCESS and evaluate --quick 0 failures at alpha.382 (earlier notes). The on-device transport of the body was not exercised: no Classic-S3 connected.
<!-- SECTION:FINAL_SUMMARY:END -->
