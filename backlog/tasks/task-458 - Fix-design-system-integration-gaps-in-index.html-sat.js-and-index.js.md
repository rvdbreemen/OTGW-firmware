---
id: TASK-458
title: 'Fix design system integration gaps in index.html, sat.js and index.js'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 22:01'
updated_date: '2026-04-27 22:08'
labels:
  - ui
  - design-system
  - bugfix
dependencies: []
references:
  - otgw_design_package/handoff.md
  - otgw_design_package/templates/index.html.template
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/sat.js
  - src/OTGW-firmware/data/index.js
  - src/OTGW-firmware/data/theme-toggle.js
  - src/OTGW-firmware/data/components.css
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Gap analysis against `otgw_design_package/handoff.md` reveals four concrete defects in the current design-system integration:

1. **`sat-state-pill` class undefined (critical)** – `index.html` and `sat.js` both use the class `sat-state-pill` for the SAT status badge. `components.css` only defines `.ds-pill`. The badge has zero styling.

2. **`theme-toggle.js` not loaded** – The file exists in `data/` but is never loaded by `index.html`. The `theme:changed` event is therefore never dispatched, and any future component that relies on it will not react to theme changes.

3. **Theme toggle button has wrong ID** – Current ID is `headerThemeToggle`; `theme-toggle.js` looks for `id="theme-toggle"`. The two systems are disconnected.

4. **Conflicting theme systems + localStorage key mismatch** – `index.js` has its own `doThemeToggle()` that saves to `'theme'`; `theme-toggle.js` saves to `'otgw-theme'`. The pre-paint script reads `'theme'`. On first run after wiring `theme-toggle.js`, the stored preference would be lost.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sat-state-pill replaced with ds-pill in both index.html and sat.js; status badge renders with correct design-system colours in light and dark mode
- [x] #2 theme-toggle.js loaded with defer in index.html head
- [x] #3 Theme toggle button has id=theme-toggle; clicking it toggles body.dark + html[data-theme] and dispatches theme:changed
- [x] #4 index.js listens for theme:changed to call SAT.setTheme / OTGraph.setTheme and persist to server; doThemeToggle() delegation removed
- [x] #5 localStorage keys unified: 'theme' and 'otgw-theme' stay in sync; pre-paint script checks both keys
- [x] #6 Build clean for both ESP8266 and ESP32-S3
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed four design-system integration gaps identified by gap-analysis against otgw_design_package/handoff.md:

**AC1 – sat-state-pill → ds-pill**
- `index.html`: changed `class="sat-state-pill is-neutral"` → `class="ds-pill"` on the status badge
- `sat.js`: replaced all five `sat-state-pill is-*` class assignments with `ds-pill is-*` (and dropped the undefined `is-neutral` modifier, falling back to bare `ds-pill`)
- Status badge now renders with correct design-system colours via `components.css` `.ds-pill.*` rules

**AC2+3 – theme-toggle.js wired up, button ID fixed**
- Added `<script src="./theme-toggle.js" defer></script>` to `index.html` head
- Changed toggle element from `<span id="headerThemeToggle">` to `<button id="theme-toggle" …>☾</button>` (proper semantic element, correct ID for theme-toggle.js)

**AC4 – index.js delegates to theme:changed**
- Removed `doThemeToggle()` function and its click/keydown delegation
- Added `document.addEventListener('theme:changed', …)` handler that: syncs `'theme'` key, calls `OTGraph.setTheme()` / `SAT.setTheme()`, updates `#darktheme` checkbox, POSTs to settings API

**AC5 – localStorage keys unified**
- Pre-paint script now reads `'otgw-theme' || 'theme'` and includes a one-time migration step: copies `'theme'` → `'otgw-theme'` if `'otgw-theme'` is unset (prevents flash-of-wrong-theme on first load after upgrade)
- `updateThemeToggle()` reads both keys
- `applyTheme()` checks both keys for "has local preference" and writes both on server-sourced apply

**Bonus – FSexplorer design-system classes**
- `<main>` → `<main class="fs-main">` (enables zebra-striping and table styling)
- `<div class="system-actions">` → `<section class="fs-action-card">` (card wrapper from design system)
- Buttons: `class='button'` → `class='button fs-btn'` / `fs-btn secondary` / `fs-btn wifi` for semantic semantic colours

**AC6 – Build**
- ESP8266: 0.77 MB ino.bin + 1.98 MB littlefs.bin ✓
- ESP32-S3: clean (prior run) ✓
<!-- SECTION:FINAL_SUMMARY:END -->
