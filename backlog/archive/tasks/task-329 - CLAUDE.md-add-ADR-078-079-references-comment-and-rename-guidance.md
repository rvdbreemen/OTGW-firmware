---
id: TASK-329
title: 'CLAUDE.md: add ADR-078/079 references + comment and rename guidance'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 17:12'
updated_date: '2026-04-19 17:21'
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
- [x] #1 CLAUDE.md ADR Guidelines lists ADR-078 (MQTT dispatch) and ADR-079 (per-component type headers) with one-liners
- [x] #2 CLAUDE.md adds: avoid defensive comments about hypothetical future scenarios
- [x] #3 CLAUDE.md adds: prefer fixing documentation over renaming identifiers when the name itself is correct
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 (ADR-078/079 references in CLAUDE.md) already landed via TASK-327 (commit 051e888f) which rewrote the ADR Guidelines section with the full binding/structural split. Only AC2+AC3 needed a fresh edit in this task: two new bullets under Design Principles -- one against hypothetical-future-scenario comments, one preferring doc fixes over renames. Both bullets reference real 2026-04-18 review incidents so the rule is not abstract.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Two new Design Principles bullets in CLAUDE.md: 'Comments about the present only' (no defensive commentary about hypothetical future scenarios) and 'Fix the doc, not the identifier' (prefer docstring fixes over renames when the name is semantically correct). ADR-078/079 references already landed via TASK-327's ADR Guidelines rewrite. Deliberately kept under Design Principles rather than a separate section so contributors see the guidance alongside KISS/YAGNI/Minimal-change-surface instead of having to hunt for it.
<!-- SECTION:FINAL_SUMMARY:END -->
