---
id: TASK-677
title: 'fix(progmem): strcmp_P consistency on sDeviceid'
status: To Do
assignee: []
created_date: '2026-05-23 16:03'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two sites in OTGW-Core.ino compare state.pic.sDeviceid with a literal "unknown" using bare strcmp. Per ADR-009 and matching the pattern at OTGW-firmware.ino:259-260, these should use strcmp_P + PSTR() to keep the literal in flash and stay consistent with the existing codebase. No behavioural change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Line ~4913 uses strcmp_P(state.pic.sDeviceid, PSTR("unknown"))
- [ ] #2 Line ~4998 uses strcmp_P(state.pic.sDeviceid, PSTR("unknown"))
- [ ] #3 python build.py --firmware exits 0
- [ ] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->
