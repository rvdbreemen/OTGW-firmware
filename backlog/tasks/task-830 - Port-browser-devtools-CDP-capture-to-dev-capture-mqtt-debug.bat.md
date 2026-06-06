---
id: TASK-830
title: Port browser-devtools (CDP) capture to dev capture-mqtt-debug.bat
status: Done
assignee:
  - '@claude'
created_date: '2026-06-06 06:13'
updated_date: '2026-06-06 06:15'
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
- [x] #1 Browser-capture code (params, Find-Browser, Start/Stop-BrowserCapture, CDP worker, browser.log merge+cleanup, -Skip/-BrowserUrl/-BrowserDebugPort/-BrowserPath switches) is present in dev script, byte-identical to 2.0.0 commit 08dce9d6
- [x] #2 Dev's TASK-828/829 additions (all-toggle-enable, Request-SettingsDump) remain intact and functional
- [x] #3 2.0.0 task-821 markdown is NOT committed onto dev (task files belong to their branch)
- [x] #4 PowerShell payload parses clean (0 AST errors)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported the headless-Edge/Chromium CDP browser-devtools capture from the 2.0.0 worktree (cherry-pick of commit 08dce9d6) onto the dev copy of scripts/capture-mqtt-debug.bat.

Mechanism: git cherry-pick -n (byte-preserving) so the browser regions stay identical to the version validated end-to-end on hardware; no hand-splice. Auto-merge was clean against dev's pre-existing all-toggle-enable + INI-dump additions (disjoint regions). The sibling worktree task markdown that rode along in the commit was removed from the index (task files belong to their branch).

Result: dev script now captures telnet + MQTT + browser.log (console.* + exceptions + Log-domain 404s + per-request network timings), merged into transcript.txt, with -SkipBrowserCapture/-BrowserUrl/-BrowserDebugPort/-BrowserPath switches and graceful degradation when no browser is found.

Verified: PowerShell AST parse clean (1452 lines); both feature sets present; old MQTT-only toggle fn absent. Committed as 7a97f539. Script-only change (no firmware); build/eval gates N/A.
<!-- SECTION:FINAL_SUMMARY:END -->
