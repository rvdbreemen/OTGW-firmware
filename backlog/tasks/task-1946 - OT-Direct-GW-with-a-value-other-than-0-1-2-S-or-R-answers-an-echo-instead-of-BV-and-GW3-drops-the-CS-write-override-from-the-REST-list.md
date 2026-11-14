---
id: TASK-1946
title: >-
  OT-Direct: GW= with a value other than 0, 1, 2/S or R answers an echo instead
  of BV, and GW=3 drops the CS write override from the REST list
status: To Do
assignee: []
created_date: '2026-11-14T19:06:00Z'
labels:
  - otdirect
  - parity
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 996000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .61 (beta.252+941a224, TASK-1839 bench, 2026-11-14). GW=3 gets the echo 'GW: 3' with no mode change and no NG/BV. The PIC 6.8 answers BadValue (BV) to any GW= value that is not 0, 1 or R (gateway.asm CheckBoolean :4072-4082, SetGatewayMode :4729-4747, ResetGateway :4776-4780); GW=2/S (master) is an OT-Direct extension. Also observed after GW=3: /api/v2/otdirect/overrides no longer listed the write override [1:10240] while PR=O still answered C40.00; cause not traced (read from 09's bench log only).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 GW= with an unsupported value answers BV, as the PIC, and changes no state; host test red on current code
- [ ] #2 The REST override list and PR=O agree after an unsupported GW= (cause of the lost [1:10240] found and fixed, or shown not reproducible)
<!-- AC:END -->
