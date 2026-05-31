---
id: TASK-773
title: 'fix(webui): expose full boiler unsupported list from truncated badge'
status: Done
assignee:
  - '@codex'
created_date: '2026-05-31 08:47'
updated_date: '2026-05-31 08:52'
labels:
  - webui
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Make the yellow 'Boiler does not implement' badge remain compact while still letting users discover the full unsupported message list when it is visually truncated. Prefer a small tooltip-style/readout interaction using the existing WebUI patterns and avoid broad layout changes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The compact yellow badge can still truncate long lists without expanding the Statistics footer layout.
- [x] #2 A user can discover and read the full unsupported list from the badge, including keyboard/focus access where practical.
- [x] #3 The empty-list behavior remains unchanged: the badge stays hidden when unsupported_read and unsupported_write are both empty.
- [x] #4 The implementation is limited to the existing WebUI files needed for the badge behavior and does not introduce a new framework.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the current badge markup, styling, and update function around #boilerUnsupportedLine.
2. Add a narrow tooltip/readout path that exposes the full generated text while preserving the compact ellipsis badge.
3. Verify the relevant source snippets and, where practical, run the existing design-system/UI checks for the touched files.
4. Update acceptance criteria and final summary when complete.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Branch: dev. Coding agent: Codex. Implemented compact discoverability for #boilerUnsupportedLine. The visible badge still truncates through .boiler-unsupported-summary, while the parent badge now owns hover/focus tooltip styling and is keyboard focusable via tabindex=0. refreshBoilerSupport() now sets data-tooltip, aria-label, and title to the full generated 'Boiler does not implement: ...' text, and clears/restores those attributes when the list is empty or the endpoint is unavailable. Touched files: src/OTGW-firmware/data/index.html, index.css, index_dark.css, index.js. Validation: node --check src/OTGW-firmware/data/index.js passed; git diff --check passed; python evaluate.py --quick reported Total Checks 36, Passed 34, Warnings 0, Failed 0, Info 2. Note: tools/check_design_system_drift.py is not present on this branch, so that older helper could not be run.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a compact tooltip/readout for the yellow 'Boiler does not implement' badge. The visible badge still ellipsizes long lists, but hover or keyboard focus now shows the full generated list in a styled tooltip; JS also exposes the full text through aria-label/title for accessibility and fallback. Empty/error states continue hiding the badge and clearing dynamic tooltip metadata. Validation passed with node --check, git diff --check, and python evaluate.py --quick.
<!-- SECTION:FINAL_SUMMARY:END -->
