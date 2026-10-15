---
id: TASK-1227
title: 'v2: deeper OT log buffer or capture mode'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-15 05:07'
updated_date: '2026-10-15 14:06'
labels:
  - webui
  - 2.0.0
  - v2
milestone: m-2
dependencies: []
priority: medium
ordinal: 365000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up port from the classic UI (decided 2026-10-15: core ports first, these after; not blocking TASK-1223). Gap audit docs/research/2026-10-15-classic-vs-v2-gap-audit.md row 21: classic keeps 2000 lines (capture mode up to 1M sized to storage quota); v2 keeps 600 (v2.js:821) and downloads only those.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 keeps at least 2000 log lines by default and offers a capture mode for longer sessions
- [x] #2 Download saves the whole buffer
- [x] #3 Bench: works on the combo unit with 0 console errors (screenshot + console log)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Bench 2026-10-15 OTGW32 (.88.61, alpha.425+4755422, safeboot layout): .tmp/bench-followups/after/ ui-*.json + png; 0 console errors in the final runs: Lines counter and Capture chip present; Capture on/off toggles; Download saves ONE file (the double wiring that saved 2 files is fixed). Default 2000 lines, capture 200k (fixed size, not storage-quota based like classic), shown in the harness.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 log keeps 2000 lines by default, a Capture mode keeps up to 200k and downloads on exit, and Download saves the whole buffer once. Evidence: bench and harness.
<!-- SECTION:FINAL_SUMMARY:END -->
