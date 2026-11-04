---
id: TASK-467
title: >-
  fix(webui): nav stacking + duplicated PIC firmware dropdown after
  design-package apply
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 05:56'
updated_date: '2026-04-28 21:28'
labels:
  - webui
  - design-system
  - bug
dependencies:
  - TASK-458
  - TASK-435
references:
  - src/OTGW-firmware/data/components.css
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/index.js
  - otgw_design_package/handoff.md
  - 'C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
After applying otgw_design_package, the rendered web UI shows five stacked Home/SAT/Settings/Advanced rows and the Advanced dropdown lists "PIC firmware" five times before the other items.

Two independent root causes compound:

1. **CSS selector mismatch** — `components.css:668` defines `.page.active { display: block; }`, but the HTML uses class `page-section`. The selector never matches, so non-active page-sections are never hidden and all render simultaneously.
2. **Half-migrated nav markup** — Two page-sections (`displayMainPage`, `displaySATPage`) use the new `<div class="page-nav-shell"></div>` slot pattern. Five page-sections (`displayPICflash`, `displayDeviceInfo`, `displaySettingsPage`, `displayWebhookPage`, `displaySATSettingsPage`) still carry legacy hardcoded `<div class="nav-container">` blocks.

`toggleHidden('adv_dropdown', false)` (index.js:3180) toggles by class, so all visible dropdowns open at once and visually stack into the "PIC firmware ×5 + Webhook + Debug + FS" sequence.

Fix:
- Replace the broken `.page.active` rule in `components.css` with `.page-section { display: none; } .page-section.active { display: block; }`.
- Replace the five legacy `<div class="nav-container">` blocks in `index.html` (lines 284, 303, 322, 343, 600) with `<div class="page-nav-shell"></div>`. The `<template id="pageNavTemplate">` + `renderSharedPageNavShell()` already handle population.
- Drop the page-specific "SAT Dashboard" label on `displaySATSettingsPage` in favour of the shared "SAT" label (single source of truth, KISS).

See plan: `C:\Users\rvdbr\.claude\plans\the-design-package-still-elegant-globe.md`. Builds on TASK-458 / TASK-435 design-package integration work.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 components.css selector matches `.page-section` (not `.page`); only the active page-section is visible in the rendered DOM
- [x] #2 All page-sections in index.html use `<div class="page-nav-shell"></div>`; no `<div class="nav-container">` exists outside the `<template id="pageNavTemplate">`
- [x] #3 Browser smoke test passes: one nav row visible, Advanced dropdown shows exactly four distinct items (PIC firmware, Webhook, Debug Information, File system contents), tab navigation works without ghosted content from other sections
- [x] #4 /design.html still renders cleanly in light and dark mode after the changes; no new console errors or 404s
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Edits applied: components.css:667-668 selector now matches .page-section. index.html: stripped 5 hardcoded nav-container blocks (displayPICflash, displayDeviceInfo, displaySettingsPage, displayWebhookPage, displaySATSettingsPage), each replaced with `<div class="page-nav-shell"></div>`. Verified: nav-container + adv_dropdown now exist only inside `<template id="pageNavTemplate">` (lines 64/72). All 7 page-sections have exactly one page-nav-shell slot (lines 85, 284, 288, 293, 298, 303, 540). Dropped page-specific 'SAT Dashboard' label per plan (single source of truth in shared template). Build running in background: id b7fyluwe2. Browser smoke test (AC3) and /design.html re-check (AC4) pending after firmware/LittleFS flash to test device.

2026-04-28: User verified the browser behavior works well: nav stacking/dropdown issue is resolved and the design page check is acceptable.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the WebUI navigation stacking and duplicated Advanced dropdown by aligning the CSS visibility selector with .page-section and replacing legacy per-page nav markup with the shared page-nav-shell pattern. User verified the browser behavior works well, so the remaining smoke-test ACs are closed.
<!-- SECTION:FINAL_SUMMARY:END -->
