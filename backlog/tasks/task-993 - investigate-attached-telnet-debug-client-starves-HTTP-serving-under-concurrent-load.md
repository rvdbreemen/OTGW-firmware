---
id: TASK-993
title: >-
  investigate: attached telnet debug client starves HTTP serving under
  concurrent load
status: To Do
assignee: []
created_date: '2026-07-03 04:34'
labels: []
dependencies: []
ordinal: 205000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed on bench S3 (TASK-989 load testing, 2026-07-03): with a telnet debug client connected, a concurrent HTTP ramp times out on EVERY request (even conc=1, 8s timeouts, zero 503s reaching the wire); detach telnet and the identical ramp returns fast 200/503/RST mixes. Hypothesis: debug writes to the attached telnet client from async/handler context block or queue-saturate the shared async serving path (AsyncSimpleTelnet shares AsyncTCP). Needs isolation: DebugTf volume vs telnet client TCP window, and whether WS suffers the same.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce with minimal case: telnet attached + conc=4 ramp -> quantify timeout rate vs detached
- [ ] #2 Identify the blocking call path (instrument or code-read AsyncSimpleTelnet write path from async context)
- [ ] #3 Fix or bound it (non-blocking telnet writes / drop-on-full) with A/B evidence
<!-- AC:END -->
