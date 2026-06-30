---
id: TASK-964
title: 'v2 parity: OT-Direct manual override apply UI (action + msgID + hex + Apply)'
status: To Do
assignee: []
created_date: '2026-06-30 23:06'
labels: []
dependencies: []
ordinal: 176000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap (grep 0 hits in v2): the v2 Home/OT-Direct surface lacks the manual-override controls the Classic UI has (Home OT-Direct panel: override action selector, message-ID input, hex value input, Apply button). Without it a user cannot send manual OT-Direct overrides from v2. POSTs to /api/v2/otdirect/overrides (the endpoint already exists; classic uses it). Action gap — a real blocker for retiring Classic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 exposes an OT-Direct override control (pick action + msgID + hex value, Apply) that POSTs /api/v2/otdirect/overrides; result reflected in the OT-Direct status
- [ ] #2 Verified on-device: an override applied from v2 takes effect (telnet/status confirms)
<!-- AC:END -->
