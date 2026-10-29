---
id: TASK-1442
title: >-
  PIC error replies (NG/BV/SE/OR) count as a confirmed command: rejected
  commands are not reported
status: To Do
assignee: []
created_date: '2026-10-29T05:14:00Z'
labels:
  - bug
  - needs-maintainer-ok
  - mqtt
  - pic
dependencies: []
priority: low
ordinal: 260000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found on the bench 2026-10-29 while verifying TASK-1431 (.68, beta.8): checkOTGWcmdqueue() matches a reply on the two command letters only, so 'ZZ: NG' (unknown command) removes ZZ=1 from the queue as confirmed and clears command_retry_exceeded. A command the PIC rejects (NG, BV, SE, OR, NS) is therefore neither retried nor reported; the user only sees it in the OT log. PR=x is the exception (register-letter match), which is why PR=Z with 'PR: BV' is retried 5 times and then reported as dropped.

Decision needed from the maintainer: should a PIC error reply be reported (for example on command_dropped with the error code, or a separate command_rejected topic), and should it still clear the retry-exceeded flag? Retrying an error reply makes no sense, so the open question is only reporting.
Evidence: C:\Users\rvdbr\AppData\Local\OTGW-capture\bench-68-beta8-20261029-0559\telnet_zz.log, telnet_drop_tp.log
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Maintainer decided whether and how a PIC error reply to a queued command is reported
- [ ] #2 An error reply no longer clears command_retry_exceeded as if the command was confirmed, or the decision explicitly keeps that behaviour
<!-- AC:END -->
