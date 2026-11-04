---
id: TASK-953
title: >-
  Make the 'New UI' switch read as a button to a new screen (classic topbar
  #tryV2)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-29 22:18'
updated_date: '2026-06-29 22:30'
labels: []
dependencies: []
ordinal: 166000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User: the '✨ New UI' control in the classic UI topbar (index.html #tryV2) does not read as a clickable button to a new frontend — it borrows .theme-toggle-btn (transparent, borderless, opacity .7, 34px icon box), so it looks like faint decoration. Restyle it as a real button using the design-system .btn .btn-primary classes (ADR-091: reuse DS classes, no drift) and add a navigation cue. updateThemeToggle() excludes #tryV2 via :not(#tryV2), so dropping the theme-toggle-btn class is safe.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 #tryV2 uses .btn .btn-primary (DS classes), renders as a filled, bordered, clearly-clickable button
- [x] #2 Label signals navigation to a new screen (e.g. arrow); aria-label/title preserved
- [x] #3 Theme toggle still works (updateThemeToggle untouched); topbar alignment intact
- [x] #4 littlefs build green; evaluate --quick (ADR-091 drift) no new failures; prerelease bumped
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DONE 2026-06-30 (alpha.294): #tryV2 changed from .theme-toggle-btn to .btn .btn-primary (DS classes, ADR-091 drift gate clean) + label '✨ Try New UI →' (nav cue). Playwright-verified on the real index.html: computed bg=rgb(33,150,243) (filled --status-info), border=1px, cursor=pointer — renders as a real button. updateThemeToggle() untouched (queries .theme-toggle-btn:not(#tryV2), so #tryV2 is unaffected). littlefs esp32-combo buildfs SUCCESS, evaluate --quick 0-fail (design-drift clean, 98.7%).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Classic topbar 'New UI' switch restyled from a faint icon (.theme-toggle-btn) to a filled DS button (.btn .btn-primary) with a navigation arrow — now clearly reads as a button to the new frontend.
<!-- SECTION:FINAL_SUMMARY:END -->
