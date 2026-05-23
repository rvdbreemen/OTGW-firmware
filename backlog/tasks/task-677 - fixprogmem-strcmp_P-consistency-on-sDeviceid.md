---
id: TASK-677
title: 'fix(progmem): strcmp_P consistency on sDeviceid'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:03'
updated_date: '2026-05-23 16:05'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two sites in OTGW-Core.ino compare state.pic.sDeviceid with a literal "unknown" using bare strcmp. Per ADR-009 and matching the pattern at OTGW-firmware.ino:259-260, these should use strcmp_P + PSTR() to keep the literal in flash and stay consistent with the existing codebase. No behavioural change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Line ~4913 uses strcmp_P(state.pic.sDeviceid, PSTR("unknown"))
- [x] #2 Line ~4998 uses strcmp_P(state.pic.sDeviceid, PSTR("unknown"))
- [x] #3 python build.py --firmware exits 0
- [x] #4 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Replace strcmp at line 4913 with strcmp_P + PSTR
2. Replace strcmp at line 4998 with strcmp_P + PSTR
3. Build firmware to verify
4. Run evaluator quick to verify
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced bare strcmp with strcmp_P(state.pic.sDeviceid, PSTR("unknown")) at two sites in src/OTGW-firmware/OTGW-Core.ino (refreshpic and upgradepic handlers). The matched literal now stays in flash, matching the ADR-009 pattern already used at OTGW-firmware.ino:259-260 and the equivalent 2.0.0 sites. No behavioural change. Verified: python build.py --firmware exits 0; python evaluate.py --quick 34 passed, 0 failed (no delta vs pre-change baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
