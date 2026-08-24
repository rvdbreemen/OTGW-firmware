---
id: TASK-1083
title: >-
  Follow-up: POST /api/v2/otgw/commands falls back to raw JSON body on over-long
  command
status: To Do
assignee: []
created_date: '2026-08-24 19:05'
labels:
  - bug
dependencies: []
priority: low
ordinal: 185000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Surfaced by the extractJsonField return-contract change in TASK-1082. At restAPI.ino:459 the /api/v2/otgw/commands handler falls back to strlcpy(cmdBuf, body.c_str(), 64) when extractJsonField returns false. With the corrected contract, a command value of 64+ chars now yields the first 63 chars of the raw JSON body instead of the first 63 chars of the command. Both are invalid PIC commands so neither reaches the queue, but the fallback is now reached for a new reason. A clean fix needs a tri-state return (found/not-found/too-long) rather than bool, which redesigns a shared helper and was deliberately left out of TASK-1082's scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 extractJsonField distinguishes not-found from too-long at its call sites, or the commands handler stops using the raw-body fallback for an over-long command value
- [ ] #2 An over-long command value produces an explicit error rather than a silently mangled command
<!-- AC:END -->
