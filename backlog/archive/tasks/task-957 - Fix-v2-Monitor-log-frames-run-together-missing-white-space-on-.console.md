---
id: TASK-957
title: 'Fix v2 Monitor log: frames run together (missing white-space on .console)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 05:04'
updated_date: '2026-07-01 19:34'
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
- [x] #1 v2 Monitor Log shows one OT frame per line (newlines honoured)
- [x] #2 .console has white-space: pre-wrap; long frames wrap instead of horizontal-scroll
- [x] #3 littlefs build green; prerelease bumped
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed in alpha.296 (commit 852d3ce05): added white-space:pre-wrap;overflow-wrap:anywhere to .console (v2.css:255) so the v2 Monitor Log renders one OT frame per line and wraps long frames, matching the classic .ot-log-content contract. renderLog() already emitted lines.join('\n'), so no JS change was needed. littlefs rebuilt + prerelease bumped. ACs reconciled against shipped code.
<!-- SECTION:FINAL_SUMMARY:END -->
