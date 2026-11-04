---
id: TASK-708
title: >-
  chore(adr-kit): adopt /adr-kit:context once available — wire into agent
  prompts
status: Done
assignee: []
created_date: '2026-05-26 11:01'
updated_date: '2026-07-09 20:46'
labels:
  - adr-kit
  - tooling
  - blocked
dependencies: []
ordinal: 74000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Blocked on rvdbreemen/adr-kit issue #7 landing.\n\nOnce bin/adr-context is available (adr-kit release after issue #7), add /adr-kit:context calls to CLAUDE.md agent-guidance for the 2.0.0 worktree. With 111 ADRs, agents currently have no efficient way to find which ADRs apply before implementing a feature. The context skill solves this.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 bin/adr-context tool is available in installed adr-kit version
- [x] #2 CLAUDE.md or agent prompts reference /adr-kit:context for feature work guidance
- [x] #3 Tested on a 2.0.0 feature task: relevant ADRs surface correctly
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09: adr-kit issue #7 landed (bin/adr-context present in the installed plugin v0.31.0). AC#1: ran bin/adr-context, returns ranked JSON. AC#2: added a bullet to CLAUDE.md 'ADR Kit Rules' instructing '/adr-kit:context <topic>' as the pre-implementation step before feature/architectural work (surfaces the 3-5 most relevant Accepted ADRs). AC#3: tested on the BLE topic ('ble scanning psram gating') — surfaces ADR-169 (PSRAM-aware BLE, 0.65) top and ADR-092 (NimBLE scanner), the correct BLE decisions. Relevant ADRs surface correctly.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wired /adr-kit:context into the CLAUDE.md ADR Kit Rules as the mandated pre-implementation step: agents now run it with the change topic to surface the most relevant Accepted ADRs before coding, instead of scanning 160+ ADRs blind. Verified the ranker returns the correct BLE ADRs.
<!-- SECTION:FINAL_SUMMARY:END -->
