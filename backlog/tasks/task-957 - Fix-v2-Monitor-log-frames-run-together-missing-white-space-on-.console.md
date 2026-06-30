---
id: TASK-957
title: 'Fix v2 Monitor log: frames run together (missing white-space on .console)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-30 05:04'
labels: []
dependencies: []
ordinal: 169000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The v2 Monitor -> Log console renders all OT frames concatenated on a continuous block instead of one frame per line. renderLog() (v2.js) correctly does el.textContent = lines.join('\n'), but .console (v2.css:252) has no white-space property, so it defaults to 'normal' and the newlines collapse to spaces. Fix: add white-space: pre-wrap to .console (matches the classic UI .ot-log-content contract; pre-wrap breaks per frame AND wraps long lines).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 v2 Monitor Log shows one OT frame per line (newlines honoured)
- [ ] #2 .console has white-space: pre-wrap; long frames wrap instead of horizontal-scroll
- [ ] #3 littlefs build green; prerelease bumped
<!-- AC:END -->
