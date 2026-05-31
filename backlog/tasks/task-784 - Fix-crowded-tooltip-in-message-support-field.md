---
id: TASK-784
title: Fix crowded tooltip in message support field
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 15:55'
updated_date: '2026-05-31 16:13'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The UI currently shows overlapping floating text when a long boiler unsupported-message list is displayed, making the field unreadable. Fix the presentation so long content remains readable without duplicate tooltip-style overlays.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Long unsupported-message content is readable without overlapping duplicate floating windows
- [ ] #2 Any expanded/tooltip behavior appears only through a deliberate hover or focus interaction
- [ ] #3 The fix is scoped to the affected UI presentation without changing boiler message data
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the UI code that renders the boiler unsupported-message field and its tooltip/overflow handling.
2. Change the presentation so long text is readable without duplicate overlays, preferring one explicit hover/focus affordance if needed.
3. Add or update focused tests/checks if this UI has static validators, then run the relevant validation commands.
4. Update TASK-784 acceptance criteria, notes, and final summary after verification.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev at ec5e5440 before the source commit.
Coding agent: @codex.
Implementation: removed the boiler unsupported banner's native title and custom data-tooltip path, cleared stale data-tooltip attributes during refresh, and changed the light/dark footer CSS so long unsupported-message text wraps inline instead of opening floating overlays.
Validation: Playwright browser render used a simulated long unsupported list at 1760x320 and 390x700; the field had title=null, dataTooltip=null, afterContent=none, white-space=normal, and overflow-wrap=anywhere. Static selector check found no boiler field title/tabindex/data-tooltip setter or boiler-specific tooltip CSS. python evaluate.py --quick passed with 34 passed, 0 failed, 2 info. python build.py passed the combined firmware plus filesystem build and produced OTGW-firmware-1.6.1-beta+ec5e544.ino.bin plus OTGW-firmware.1.6.1-beta+ec5e544.littlefs.bin. Build cleanup warned that .tmp could not be fully removed because an existing echarts pack file returned WinError 5.
<!-- SECTION:NOTES:END -->
