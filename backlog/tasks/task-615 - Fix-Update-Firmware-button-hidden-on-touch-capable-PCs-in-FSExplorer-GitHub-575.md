---
id: TASK-615
title: >-
  Fix: Update Firmware button hidden on touch-capable PCs in FSExplorer (GitHub
  #575)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-17 07:07'
updated_date: '2026-05-18 17:10'
labels:
  - bug
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/575'
  - 'GitHub reporter: rkuijer'
  - '2026-05-15'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 'Update Firmware' button in FSExplorer is hidden on devices that report as touch-capable (pointer: coarse, hover: none) even when used as a desktop PC — e.g., Microsoft Surface Pro 9. Root cause identified by reporter rkuijer: the button's container uses class='desktop-only' which is hidden by the CSS media query '@media (pointer: coarse) and (hover: none)'. Maintainer (rvdbreemen) acknowledged the bug and committed to backlogging it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Update Firmware button is visible on touch-capable PCs (e.g. Surface Pro 9) running Chrome and Firefox
- [x] #2 CSS fix does not regress the mobile-hide behaviour for genuinely small-screen touch devices (phones/tablets)
- [x] #3 FSExplorer HTML/CSS change compiles cleanly (python build.py --firmware exits 0)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Tighten the existing FSexplorer hide rule to (pointer: coarse) and (hover: none) and (max-width: 900px) in FSexplorer.css and FSexplorer_dark.css; update the comment.
2. No FSexplorer.html / JS / UA-sniffing change (drop PR #577 approach).
3. Version bump beta.10 -> beta.11 via bin/bump-prerelease.sh; stage version.h + data/version.hash.
4. build.py --firmware (exit 0) + evaluate.py --quick (no new failures).
5. Commit to claude/review-pr-577-rhMza, push, open draft PR superseding #577.
6. Check AC#1-3, Final Summary, set Done.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Bounded the FSexplorer hide rule to small touch-only screens, fixing GitHub #575 (Update Firmware button vanished on Surface-class PCs in tablet mode).

Change: `@media (pointer: coarse) and (hover: none)` -> `... and (max-width: 900px)` in FSexplorer.css and FSexplorer_dark.css (comment rewritten too). Surface Pro (~912px+ even portrait) keeps the button; phones/portrait tablets (<=900px, touch-only) still hide it; mouse desktops never match pointer:coarse so resized-narrow windows are unaffected. No UA sniffing/JS (rejected PR #577 approach as fragile + larger surface).

Known accepted trade-off: landscape tablets >900px still show the button (Surface-portrait 912 vs phone-landscape 932 overlap makes a clean CSS threshold impossible; 900 chosen to never re-break Surface). No 2.0.0 port needed (different CSS arch, width-based, no Surface bug).

Validation: build.py --firmware exit 0 (beta.11 artifact); evaluate.py --quick 34 pass / 0 fail / 0 warn. Prerelease bumped beta.10 -> beta.11. Draft PR #598 (supersedes #577).
<!-- SECTION:FINAL_SUMMARY:END -->
