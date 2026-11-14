---
id: TASK-1951
title: >-
  OT-Direct PR=M answers S/P/L, which handlePRresponse logs as unexpected and
  ignores
status: To Do
assignee: []
created_date: '2026-11-14T22:57:00Z'
labels:
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 996000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on .61 (beta.264, OT-Direct, no boiler) 2026-11-14: MQTT command PR=M gives telnet 'PR: M=S' and handlePRresponse logs 'PR=M unexpected value [S]'. Cause (95, from code): OT-Direct reports its own modes in PR: M= (OTDirect.ino:3857-3865: G, M, P=bypass, S=master/standalone, L=loopback), while handlePRresponse (OTGW-Core.ino:1367-1379) accepts only G/M (the PIC only knows G/M), logs 'unexpected value' for anything else and returns without updating the gateway-mode state. Not caused by TASK-1848. Decision needed: the consumer accepts the OT-Direct extensions S/P/L, or OT-Direct reports G/M like the PIC (pyotgw compatibility).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decide (coordinator/Robert): the consumer accepts S/P/L, or OT-Direct answers PR=M with G/M like the PIC
- [ ] #2 Host test on the real handlePRresponse / OT-Direct PR=M path: red on the current code, green on the fix; no 'unexpected value' for a valid OT-Direct mode
- [ ] #3 Bench: PR=M on an OT-Direct unit updates the gateway-mode state without the warning
<!-- AC:END -->
