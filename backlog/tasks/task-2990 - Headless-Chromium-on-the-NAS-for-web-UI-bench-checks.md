---
id: TASK-2990
title: Headless Chromium on the NAS for web UI bench checks
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T02:06:00Z'
updated_date: '2026-11-21T05:01:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1118000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-21: browser/CDP checks against bench units also run on the NAS, not on the laptop. Image otgw-bench-chromium:alpine3.21 (alpine 3.21 + chromium + nodejs, Robert's OK for the download), runner uicheck.sh, first script ui/task2982.mjs; recipe in docs/guides/agent-build-and-bench.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Image builds on the NAS and a CDP WebSocket opens from inside the container
- [x] #2 uicheck.sh runs a script against a unit and writes facts.json and screenshots to runs/<run>/
- [x] #3 scripts/nas-bench/ holds Dockerfile, uicheck.sh and ui/task2982.mjs equal to the NAS copies; the guide documents the flags and the start time
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21: Robert OK for the download (alpine:3.21 + chromium + nodejs). Image built on the NAS, pinned (alpine digest bc214871..., chromium 136.0.7103.113-r0, nodejs 22.23.5-r0, font-noto 24.7.1-r0, font-dejavu 2.37-r5), 837 MB. Chromium hangs at start with its zygote on the NAS kernel 4.4.302; --single-process starts but the DevTools endpoint is flaky; --no-zygote --in-process-gpu works: DevTools up after ~35 s, CDP WebSocket opens (raw node test 'WS OPEN'). uicheck.sh t2982-ui-63 192.168.88.63 ui/task2982.mjs: exit 0, facts.json + 2 screenshots (TASK-2982 checks 2-4). sha256 repo = NAS for Dockerfile, uicheck.sh, ui/task2982.mjs.

2026-11-21: second script ui/task2991.mjs (rail keeps focus after Enter/Space, #setSearch keeps focus while filtering, TASK-2982 regression), deployed to bin/ui/, runs after the 08:00 beta soak.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: image builds on the NAS (pinned), CDP WebSocket opens inside the container. AC2: uicheck.sh ran TASK-2982 checks 2-4 against .63, exit 0, facts.json + screenshots in runs/t2982-ui-63 (copied to OTGW-field-data with SHA256SUMS). AC3: scripts/nas-bench/{chromium/Dockerfile,uicheck.sh,ui/task2982.mjs} equal to the NAS copies; guide section documents the flags, the ~35 s start and the Enter-keypress detail.
<!-- SECTION:FINAL_SUMMARY:END -->
