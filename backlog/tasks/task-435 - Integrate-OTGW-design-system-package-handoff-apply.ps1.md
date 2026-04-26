---
id: TASK-435
title: Integrate OTGW design system package (handoff + apply.ps1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-26 23:23'
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
- [ ] #1 Reusable skill installed at .claude/skills/firmware-design-handoff/SKILL.md so future Claude Code sessions can invoke it via the standard skill flow
- [ ] #2 apply.ps1 ran successfully (or equivalent file-copy was performed) and a timestamped backup zip exists in otgw_design_package/backups/
- [ ] #3 Package files (ds-tokens.css, components.css, theme-toggle.js, sat-slider.js, echarts-theme.js, design.html, fonts/*) live in src/OTGW-firmware/data/
- [ ] #4 Patch A (wire up): index.html links ds-tokens.css and components.css plus the three new JS files; old index.css / index_dark.css links remain for the cascade-shadowing window
- [ ] #5 Patch B (SAT panel): sat.html.template block applied; sat.js dispatches sat:rendered, uses the new state-pill classes, and wraps echarts.init via otgwChartTheme()
- [ ] #6 Patch C (log viewer): markup audit confirms classes already match the design tokens; only edits if firmware actually emits different IDs/classes
- [ ] #7 Patch D (settings + FSexplorer): templates reviewed against existing markup; optional cards/sticky-save improvements applied at discretion
- [ ] #8 Patch E (drop legacy): old index.css / index_dark.css / FSexplorer*.css removed once A-D verified; LittleFS image size drops accordingly
- [ ] #9 python build.py exits 0 for both ESP8266 and ESP32 targets after each patch lands
- [ ] #10 /design.html on the device renders every component in light + dark; theme toggle swaps every page in one click; no DevTools 404s or CSS warnings
<!-- AC:END -->
