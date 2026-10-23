---
id: TASK-1324
title: >-
  SAT write-on-change cache can skip a CH/MM resend after an OT online edge
  early after boot
status: To Do
assignee: []
created_date: '2026-10-23T15:57:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 447000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1316 review (pre-existing): satResetCmdCache() zeroes iLastSentCHMs but keeps iLastSentCH; within the same tick satEnqueueIfChangedCS sets the shared bLastSentValid=true before the CH helper runs. With uptime under SAT_CMD_REFRESH_MS (5 min) the CH value equal to the cached one is not 'stale' and is skipped, e.g. CH=1 after an OT offline->online edge shortly after boot, although the reset meant to force a resend. Fix: per-field valid flags, or set the *Ms fields to now - SAT_CMD_REFRESH_MS on reset.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test reproduces the skipped resend after a cache reset early after boot (red), fix sends it (green)
<!-- AC:END -->
