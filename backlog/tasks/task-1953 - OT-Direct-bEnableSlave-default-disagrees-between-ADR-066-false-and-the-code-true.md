---
id: TASK-1953
title: >-
  OT-Direct: bEnableSlave default disagrees between ADR-066 (false) and the code
  (true)
status: Done
assignee: []
created_date: '2026-11-14T23:39:00Z'
updated_date: '2026-11-15T08:01:00Z'
labels:
  - otdirect
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
- [x] #1 Robert decided the default (true or false) and, for a change, what happens to stored values
- [x] #2 Code, settings help and ADR record agree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 Robert decided A: bEnableSlave keeps the code default true (PIC-like: the PIC always serves the thermostat side); ADR-066's text (false) gets a small amending ADR, Proposed via adr-kit, then Accepted on this decision. Discord DM 1571432519373557801, answer to the coordinator's question 1571432372916850780 (item 4, which also set OH= default 1 for new installs; that part is not this task). No settings migration, no code change. Docs-only, pushed after 'beta tag out'.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Robert decided A (keep the code default true), Discord DM 1571432519373557801, 2026-11-15. AC2: code (OTDirecttypes.h:114 'bEnableSlave = true'), settings help (settingsHelp.h:90 'Default on') and the ADR record now agree: ADR-243 (Accepted 2026-11-15, amends ADR-066's stated default; strict adr-lint clean, tests/test_adr_governance.py 17 OK). No code change, no migration.
<!-- SECTION:FINAL_SUMMARY:END -->
