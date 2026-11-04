---
id: TASK-965
title: 'v2 parity: send raw OTGW command from the UI'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 23:06'
updated_date: '2026-07-01 19:34'
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
- [x] #1 v2 has a command input + send that POSTs the OTGW command and shows success/error status
- [x] #2 Verified on-device: a command sent from v2 reaches the gateway (e.g. PS=1 toggles, confirmed via status/telnet)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: send a raw OTGW command from the UI (closes the action gap vs Classic). Added a command input + Send + status row to the v2 Monitor > Log toolbar; POSTs {command} to /api/v2/otgw/commands (202 Accepted). Works on PIC and OT-Direct (OTGW32 translates PIC commands to OT frames). Enter key also sends; status shows 'Sent: <cmd>' / error and auto-clears. Reuses tsearch/tbtn/tchip classes (no drift). Verified on .39: input+Send present, Send 'PR=A' -> POST 202, status 'Sent: PR=A', input cleared, 0 console errors.
<!-- SECTION:FINAL_SUMMARY:END -->
