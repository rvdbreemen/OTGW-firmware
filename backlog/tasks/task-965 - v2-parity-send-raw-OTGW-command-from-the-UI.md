---
id: TASK-965
title: 'v2 parity: send raw OTGW command from the UI'
status: To Do
assignee: []
created_date: '2026-06-30 23:06'
labels: []
dependencies: []
ordinal: 177000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep ~0 hits in v2): Classic has an OT-command input + Send button + status in the Log toolbar (POST /api/v2/otgw/command/<cmd>). v2 has no way to send a raw OTGW command (PS=1, TT=20.5, etc.). Power-user action gap; blocker for retiring Classic. Note otgw/commands is POST + validated (405 on GET, 400 on empty).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 has a command input + send that POSTs the OTGW command and shows success/error status
- [ ] #2 Verified on-device: a command sent from v2 reaches the gateway (e.g. PS=1 toggles, confirmed via status/telnet)
<!-- AC:END -->
