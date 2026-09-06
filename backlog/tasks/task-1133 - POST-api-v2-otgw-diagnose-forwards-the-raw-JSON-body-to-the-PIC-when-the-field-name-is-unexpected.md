---
id: TASK-1133
title: >-
  POST /api/v2/otgw/diagnose forwards the raw JSON body to the PIC when the
  field name is unexpected
status: To Do
assignee: []
created_date: '2026-09-06 19:18'
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
- [ ] #1 A POST with an unexpected or missing field returns a 4xx and writes nothing to the PIC
- [ ] #2 If a bare-body form is deliberately supported, it passes the same printable+CR filter as the parsed path
- [ ] #3 The relationship to TASK-1083 is settled: either one shared fix or an explicit note that the two paths differ
<!-- AC:END -->
