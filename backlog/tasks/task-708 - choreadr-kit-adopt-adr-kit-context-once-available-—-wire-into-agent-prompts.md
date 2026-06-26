---
id: TASK-708
title: >-
  chore(adr-kit): adopt /adr-kit:context once available — wire into agent
  prompts
status: Done
assignee: []
created_date: '2026-05-26 11:01'
updated_date: '2026-06-01 20:52'
labels:
  - adr-kit
  - tooling
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
