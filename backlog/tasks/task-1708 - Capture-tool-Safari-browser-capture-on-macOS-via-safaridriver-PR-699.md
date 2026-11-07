---
id: TASK-1708
title: 'Capture tool: Safari browser capture on macOS via safaridriver (PR #699)'
status: Done
assignee: []
created_date: '2026-11-07T18:26:00Z'
updated_date: '2026-11-07T18:28:00Z'
labels:
  - capture-tool
  - macos
  - community
dependencies: []
ordinal: 814000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Community PR #699 (Crash-Evans), squash-merged into dev as 460286f24. otgw_capture.py drives Safari through Apple's safaridriver (W3C WebDriver, stdlib only) when no Edge/Chrome/Chromium is found on macOS. --browser auto|chromium|safari, alias -Browser. Hook script injected after page load (console, errors, fetch/XHR, WebSocket); earlier requests via Resource Timing; only gateway requests listed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 --browser safari records console, errors, fetch/XHR and WebSocket events into browser.log
- [x] #2 auto picks Edge/Chrome/Chromium first and Safari only on macOS without them; Windows/Linux behaviour unchanged
- [x] #3 the WebDriver session and safaridriver are closed at stop
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Review 2026-11-07 (Claude for Robert): full diff read. Windows/Linux path unchanged apart from the skipped-message text; only gateway requests logged, no bodies (no settings or password in browser.log); session DELETE + safaridriver terminate at stop; profile_dir cleanup guarded. Evidence is the author's: macOS 27.3 / Safari 27.3 runs (120 s safari, 60 s auto, 53 [net] lines all 200) and test t with a fake safaridriver. NOT verified here: test t skips on Windows (os.name == 'nt') and CI does not run the Python tests; the PR code was not executed on this machine. Author reports FrameGapCase.test_ws_drop_leaves_a_gap_marker_and_an_index_gap failing on unmodified dev on macOS; on Windows it passes on dev c928bca (2 of 2 runs), so likely macOS-only, not followed up here. claude-review check is red on fork PRs (no OIDC token for forks), not caused by this code.
<!-- SECTION:NOTES:END -->
