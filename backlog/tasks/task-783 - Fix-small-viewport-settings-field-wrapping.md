---
id: TASK-783
title: Fix small viewport settings field wrapping
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-31 15:08'
updated_date: '2026-05-31 15:16'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On narrow screens, settings rows currently keep label text and input/control on the same line in places where the responsive layout should stack them. Scope the fix to small viewport rendering only so desktop layout remains unchanged.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 At the small viewport breakpoint, label text renders on its own line before the associated input/control.
- [ ] #2 Each stacked setting keeps the input/control before the next card or row begins.
- [ ] #3 Desktop settings layout remains unchanged.
- [ ] #4 Validation covers the affected responsive CSS/markup path.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the settings page markup and responsive CSS controlling label/control rows.
2. Apply a small-screen-only rule so setting labels and controls stack cleanly without changing desktop layout.
3. Validate with targeted mobile/desktop rendering checks and update TASK-783 acceptance criteria, notes, and final summary.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch metadata: branch `dev`, upstream `origin/dev`, implementation commit `2f967ae9`.
Coding agent: Codex.
Implementation notes: The mobile settings layout did not stack because the desktop selector `.settingDiv:not(.fixed-ip-section)` has higher specificity than the mobile `.settingDiv` override. Updated the max-width 768px rule in `src/OTGW-firmware/data/index_common.css` to override at matching specificity and made `.settings-field-container` block-level inside that breakpoint so labels occupy their own line before inputs/controls. Desktop flex layout remains governed by the unchanged top-level rule.
Validation: Rendered a Playwright mobile fixture at 390x844 using the production CSS and verified label -> input -> next card stacking. Rendered the same fixture at 1000x700 and verified desktop label/input alignment remains side-by-side. Ran `python evaluate.py --quick`; result was 36 total checks, 34 passed, 0 warnings, 0 failed, 2 info, health score 100%.
<!-- SECTION:NOTES:END -->
