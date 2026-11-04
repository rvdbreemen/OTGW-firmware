---
id: TASK-970
title: 'v2 parity: Log stream-to-file + SAT-only filter + auto-download'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 23:10'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 182000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Verified gap: Classic Log has stream-to-file (0 hits in v2), SAT-only filter (0 hits), auto-download 15min (1 hit). v2 Log lacks these three (capture mode IS present).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 Log adds a SAT-only filter toggle
- [x] #2 v2 Log adds stream-to-file + auto-download
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2 parity: Log power-features + fixed two dead buttons. Added SAT-only filter chip (keeps only 'HH:MM:SS S ' SAT-narration lines in renderLog), Auto-download chip (downloads the log buffer every 15 min), and Stream-to-file chip (File System Access API showSaveFilePicker -> writes every frame to a local file as it arrives, even while the display is Paused; Chrome/Edge only, feature-detected + dimmed elsewhere). ALSO wired the v2 Clear + Download buttons, which were rendered but had NO click handlers (dead). Verified on .39: all 5 controls present, SAT-only + Auto-download toggle on, Download produced otgw-log-<ts>.txt, 0 console errors. Reuses tchip/tbtn (no drift).
<!-- SECTION:FINAL_SUMMARY:END -->
