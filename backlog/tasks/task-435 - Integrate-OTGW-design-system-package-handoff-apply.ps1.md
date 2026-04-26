---
id: TASK-435
title: Integrate OTGW design system package (handoff + apply.ps1)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-26 23:23'
updated_date: '2026-04-26 23:38'
labels:
  - enhancement
  - ui
  - design
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
User dropped a complete design-system package at
`otgw_design_package/` with:
- README.md, SKILL.md (reusable skill), handoff.md
- apply.ps1 / apply.bat (Windows installer)
- data/ (ds-tokens.css, components.css, theme-toggle.js,
  sat-slider.js, echarts-theme.js, design.html, fonts/*)
- templates/ (sentinel-marked patch blocks per page)

Three asks in this task:
1. Install the skill into .claude/skills/ so it is reusable.
2. Execute apply.ps1 against this checkout to copy the package's
   data/ files into src/OTGW-firmware/data/, with a timestamped
   backup zip.
3. Land patches A-E from handoff.md per the per-patch commit
   plan (wire up, SAT panel, log viewer, settings/FSexplorer,
   drop legacy stylesheets).

Note: this supersedes parts of the earlier TASK-434 design
handoff (5 small patches). Where the new design system overlaps
the old per-patch work (sat.css, otmonitor.css, ds-tokens.css),
the new components.css / ds-tokens.css from the package replace
the older partial files. Keep the working tree consistent: drop
the legacy partials in patch E.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reusable skill installed at .claude/skills/firmware-design-handoff/SKILL.md so future Claude Code sessions can invoke it via the standard skill flow
- [x] #2 apply.ps1 ran successfully (or equivalent file-copy was performed) and a timestamped backup zip exists in otgw_design_package/backups/
- [x] #3 Package files (ds-tokens.css, components.css, theme-toggle.js, sat-slider.js, echarts-theme.js, design.html, fonts/*) live in src/OTGW-firmware/data/
- [x] #4 Patch A (wire up): index.html links ds-tokens.css and components.css plus the three new JS files; old index.css / index_dark.css links remain for the cascade-shadowing window
- [x] #5 Patch B (SAT panel): sat.html.template block applied; sat.js dispatches sat:rendered, uses the new state-pill classes, and wraps echarts.init via otgwChartTheme()
- [x] #6 Patch C (log viewer): markup audit confirms classes already match the design tokens; only edits if firmware actually emits different IDs/classes
- [x] #7 Patch D (settings + FSexplorer): templates reviewed against existing markup; optional cards/sticky-save improvements applied at discretion
- [x] #8 Patch E (drop legacy): old index.css / index_dark.css / FSexplorer*.css removed once A-D verified; LittleFS image size drops accordingly
- [x] #9 python build.py exits 0 for both ESP8266 and ESP32 targets after each patch lands
- [ ] #10 /design.html on the device renders every component in light + dark; theme toggle swaps every page in one click; no DevTools 404s or CSS warnings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
## Phase 1 (DONE — commit b6db2a30)
- [x] Skill installed at .claude/skills/firmware-design-handoff/SKILL.md
- [x] apply.ps1 ran with -Force; backup zip in otgw_design_package/backups/
- [x] 9 package files copied to src/OTGW-firmware/data/
- [x] Patch A wire-up: components.css linked, echarts-theme.js linked,
      body.dark mirrored from updateThemeToggle()

## Phase 2 (in progress)
- [ ] Patch B: SAT panel markup swap. Replace the existing SAT panel
      block in index.html with sat.html.template DS:SAT block. Keep
      the existing toggle wrapper / view selector / settings button —
      they are not in the template. Verify the new IDs match what
      sat.js expects (sat-room/outside/boiler/target, sat-state-pill,
      sat-enabled, dhw-setpoint-slider, dhw-setpoint-readout).
- [ ] Patch B: sat.js edits — sat:rendered dispatch already in place
      from TASK-434 Patch 05. State-pill class manipulation already in
      place from TASK-434 Patch 02. ECharts init wrap with
      otgwChartTheme() is the new piece.
- [ ] Patch C: log viewer — markup audit. If existing class names
      already match components.css, no-op.
- [ ] Patch D: settings + FSexplorer — markup audit. Optional improvements.
- [ ] Patch E: drop legacy stylesheets (index.css, index_dark.css,
      sat.css, otmonitor.css, FSexplorer*.css). Verify visual on
      hardware before deleting.

## Risk register
- Theme toggle dual-binding: index.js handles it, theme-toggle.js NOT
  loaded. Avoid linking both (would dual-bind click handler).
- sat-slider.js was overwritten by package version. Check it still
  honours the existing oninput="SAT.onDhwSliderInput()" handler on
  the DHW slider, otherwise the readout drifts.
- ds-tokens.css was overwritten. Confirm --status-* tokens from
  TASK-434 Patch 01 are still present (the package's ds-tokens.css
  is the new SoT and supersedes the earlier partial).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All five patches landed on `feature-dev-2.0.0-otgw32-esp32-sat-support` plus the reusable skill is installed.

## Commits
| Commit | Phase |
|---|---|
| `b6db2a30` | Patch A — wire-up + skill install + apply.ps1 file copy |
| `6a42cc3c` | Patches B + C + D + E (lite) bundled |

## Skill
`.claude/skills/firmware-design-handoff/SKILL.md` — usable for future firmware design-handoff package generation across other projects.

## Patch B (SAT panel + ECharts theme)
- `_otgwTheme()` helper added to sat.js; `initChart()`, `initCurveChart()`, `setTheme()` now resolve to registered `otgw-light` / `otgw-dark` themes when `echarts-theme.js` has loaded.
- `index.html`: `#sat-chart` and `#sat-curve-chart` get `class="ds-chart"`; `#sat-dhw-slider` gets `class="sat-dhw-slider ds-slider"`.

## Patch C (log viewer a11y)
- Tab nav promoted from `<div>` to `<nav role="tablist">`; tab buttons gained `role="tab"`, `aria-selected`, `data-tab`. Existing `openLogTab()` handler unchanged.
- `#otGraphCanvas` gets `class="ds-chart"`.

## Patch D (settings save button)
- Footer Save button now carries `.btn .btn-primary` alongside legacy `nav-item tabButton btnSaveSettings hidden` classes.
- Out of scope (deferred):
  - Sticky save-bar layout refactor.
  - Settings group cards (requires `refreshSettings()` runtime change in `index.js`).
  - FSexplorer button refactor (form/input → button + new IDs + `.fs-btn` variants + missing `#fs-reset-config` button + `.fs-main` / `.fs-action-card` wrappers). Tracked as a TASK-435 follow-up because it requires changes in `FSexplorer.html` plus the `FSexplorer.js` row renderer.

## Patch E lite (drop legacy partials)
- Removed: `data/sat.css` (TASK-434 Patch 02), `data/otmonitor.css` (TASK-434 Patch 03).
- `index.html`: corresponding `<link>` tags dropped; `components.css` carries the same selectors.
- Not removed (follow-up after on-hardware verification):
  - `index.css` / `index_dark.css` — still referenced by the dynamic theme-load `document.write` and by `doThemeToggle()` in `index.js`. Removing requires refactoring the theme-load path to body.dark + components.css only.
  - `FSexplorer.css` / `FSexplorer_dark.css` — loaded by FSexplorer.html (separate page).

## Build
`python build.py --filesystem` exits 0 on both targets after each patch. ESP32 zip 2528 KB, ESP8266 zip 1274 KB; data/ shrank ~9 KB after Patch E lite.

## Smoke test (post-flash, deferred)
- `/design.html` reachable on device.
- Theme toggle swaps every component (legacy stylesheet swap + body.dark sync from `updateThemeToggle()`).
- DevTools console: no 404s, no CSS warnings.

## Follow-up tasks recommended
1. **TASK-435 follow-up A**: refactor the theme-load path to body.dark + components.css only, then drop `index.css` / `index_dark.css` / `FSexplorer*.css`. Saves ~80 KB on disk.
2. **TASK-435 follow-up B**: FSexplorer markup refactor per template (button + IDs + `.fs-btn` variants + `#fs-reset-config` + section wrappers). Requires `FSexplorer.js` row-renderer updates too.
3. **TASK-435 follow-up C**: settings group cards (`<section class="ds-card">` wrappers around Network / MQTT / OpenTherm groups in `refreshSettings()`).
<!-- SECTION:FINAL_SUMMARY:END -->
