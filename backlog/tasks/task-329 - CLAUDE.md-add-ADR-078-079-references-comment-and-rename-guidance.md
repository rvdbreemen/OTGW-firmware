---
id: TASK-329
title: 'CLAUDE.md: add ADR-078/079 references + comment and rename guidance'
status: To Do
assignee: []
created_date: '2026-04-19 17:12'
labels:
  - documentation
  - process
  - review-2026-04-18-followup
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three small additions flagged by the 2026-04-18 review retrospective: (a) list ADR-078 (MQTT dispatch tables) and ADR-079 (per-component type headers) in the ADR Guidelines section so contributors see them as binding; (b) add a one-line rule under In code style that bans defensive comments about hypothetical scenarios (real incident in this review: overcautious comment in SATcontrol.ino about a future simulation preview mode that does not exist); (c) add rename-vs-docstring guidance (a rename touching N call sites is rarely a win when the existing identifier is semantically correct and only the doc is stale).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 CLAUDE.md ADR Guidelines lists ADR-078 (MQTT dispatch) and ADR-079 (per-component type headers) with one-liners
- [ ] #2 CLAUDE.md adds: avoid defensive comments about hypothetical future scenarios
- [ ] #3 CLAUDE.md adds: prefer fixing documentation over renaming identifiers when the name itself is correct
<!-- AC:END -->
