---
id: TASK-1953
title: >-
  OT-Direct: bEnableSlave default disagrees between ADR-066 (false) and the code
  (true)
status: To Do
assignee: []
created_date: '2026-11-14T23:39:00Z'
labels:
  - otdirect
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 999000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-1812 design (d3, 2026-11-15). ADR-066 (Accepted) says 'bEnableSlave = false (default): Pure standalone master'; the code has bool bEnableSlave = true (src/OTGW-firmware/OTDirecttypes.h:114, 'Enable slave interface in master mode'), and settingsHelp.h says 'Default on'. With true, a master-mode unit with a thermostat on the slave port answers it as a virtual boiler (ADR-066) and, after TASK-1812/ADR-241, relays its writes. Decide which default is right. Changing the code default is a settings migration question (a stored value stays; only units without the key change); keeping true means ADR-066's text is wrong and needs a superseding or amending ADR (Accepted ADRs are immutable).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decided the default (true or false) and, for a change, what happens to stored values
- [ ] #2 Code, settings help and ADR record agree
<!-- AC:END -->
