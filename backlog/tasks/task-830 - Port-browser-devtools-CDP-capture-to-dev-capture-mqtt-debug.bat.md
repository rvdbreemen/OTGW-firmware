---
id: TASK-830
title: Port browser-devtools (CDP) capture to dev capture-mqtt-debug.bat
status: To Do
assignee: []
created_date: '2026-06-06 06:13'
labels:
  - tooling
  - diagnostics
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the headless-Edge/Chromium CDP browser-log capture (console.* + exceptions + Log-domain 404s + network timings, merged into transcript.txt) from the 2.0.0 worktree (commit 08dce9d6, TASK-821) into the dev copy of scripts/capture-mqtt-debug.bat. Dev already has the all-toggle-enable + INI-dump additions (TASK-828/829); this adds browser capture on top. Byte-preserving port (cherry-pick) to keep TASK-821's end-to-end hardware validation valid; no hand-splice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Browser-capture code (params, Find-Browser, Start/Stop-BrowserCapture, CDP worker, browser.log merge+cleanup, -Skip/-BrowserUrl/-BrowserDebugPort/-BrowserPath switches) is present in dev script, byte-identical to 2.0.0 commit 08dce9d6
- [ ] #2 Dev's TASK-828/829 additions (all-toggle-enable, Request-SettingsDump) remain intact and functional
- [ ] #3 2.0.0 task-821 markdown is NOT committed onto dev (task files belong to their branch)
- [ ] #4 PowerShell payload parses clean (0 AST errors)
<!-- AC:END -->
