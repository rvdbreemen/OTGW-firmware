---
id: TASK-772
title: Investigate boiler warning alert truncation
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 08:37'
updated_date: '2026-05-31 08:42'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Investigate why the yellow boiler warning alert in the web UI truncates text like 'Boiler does not implement: ...' and report the root cause without changing UI code unless separately requested.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Root cause is traced to the relevant markup, JavaScript, or CSS rule.
- [x] #2 No source changes are made as part of this investigation unless explicitly requested.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the OT Support alert markup and CSS for the warning message shown as 'Boiler does not implement: ...'.
2. Identify the exact truncation rule and explain why the browser renders an ellipsis there.
3. Keep the investigation read-only unless the user explicitly asks for a UI change.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev. Coding agent: Codex. Read-only investigation; no source files changed. Root cause: #boilerUnsupportedLine in src/OTGW-firmware/data/index.css is styled with max-width: 75%, overflow: hidden, text-overflow: ellipsis, and white-space: nowrap. index.js still writes the full unsupported message list into #boilerUnsupportedList; the browser truncates the rendered inline-block when it exceeds the CSS max width. Related prior task TASK-706 explicitly added this badge styling with ellipsis.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Read-only investigation completed on branch dev. The yellow 'Boiler does not implement' badge truncates because TASK-706 added ellipsis styling to #boilerUnsupportedLine in src/OTGW-firmware/data/index.css: max-width: 75%, overflow: hidden, text-overflow: ellipsis, and white-space: nowrap. The JavaScript populates the full list, but the browser intentionally clips the visible single-line badge when the text exceeds the allowed width. No source changes were made.
<!-- SECTION:FINAL_SUMMARY:END -->
