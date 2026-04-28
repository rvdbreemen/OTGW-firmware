---
id: TASK-475
title: 'feat(webui): write CSS for SAT post-Patch-E features (~30 unstyled classes)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-28 17:27'
updated_date: '2026-04-28 17:43'
labels:
  - webui
  - design-system
  - sat
  - design-work
dependencies:
  - TASK-471
  - TASK-472
  - TASK-473
references:
  - src/OTGW-firmware/data/components.css
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/sat.js
  - tools/check_design_system_drift.py
  - otgw_design_package/AUDIT-2026-04-28.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write design-system CSS for SAT classes that were introduced after the design-package's `sat.css` was deleted in Patch E (TASK-435 commit `6a42cc3c`). These classes have NEVER been styled — they were added to HTML/JS in commit `4fe0cedd` and later, without companion CSS.

Unlike TASK-472 which mechanically ports from existing source, this task requires **design decisions**: layout, spacing, colour usage from tokens.

## Pre-conditions

- TASK-471 (cyan tokens) is committed — so token values are stable when authoring rules
- TASK-472 (mechanical port) is committed — so the legacy SAT pre-Patch-E classes (e.g. `.sat-tile`, `.sat-state-pill`, `.sat-dhw-slider`) provide the visual style baseline to harmonise with
- TASK-473 (density) is committed — so density tokens have their final values

## Context

Drift detector identified ~30 SAT classes that have no CSS source in git history:
- `sat-section` (14x), `sat-controls-row` (3x), `sat-card-controls` (1x)
- `sat-grid` (8x), `sat-row` (50x), `sat-label` (50x), `sat-value` (48x), `sat-btn` (17x)
- `sat-btn-preset` (7x), `sat-btn-mode` (4x), `sat-btn-temp` (2x), `sat-btn-toggle` (2x)
- `sat-indicator` (10x), `sat-indicator-off` (7x)
- `sat-health-grid` (1x), `sat-health-item` (6x), `sat-health-dot` (7x)
- `sat-curve-info` (1x), `sat-curve-toggle` (1x)
- `sat-settings-group / -header / -body / -toggle / -arrow / -nav-btn / -save-btn` (1x each)
- `sat-setting-input` (4x), `sat-setting-changed` (1x), `sat-setting-unit` (1x)
- `sat-status-summary` (1x), `sat-simple-status` (2x), `sat-feedback` (3x)
- `sat-toggle-label` (2x), `sat-enable-toggle-wrap` (1x)
- `sat-collapsible` (2x), `sat-dashboard` (2x), `sat-raw-data` (1x)
- `sat-badge` (2x), `sat-badge-sim` (2x)
- `sat-view-simple / sat-view-expert / sat-view-` (1x each)
- `sat-weather-coords` (1x)

(Definitive list at completion of TASK-472 — re-run drift detector to refresh.)

## Approach

1. **Audit existing SAT markup** — read `index.html` SAT page sections (around lines 306-540), `sat.js` and the visual screenshots to understand intended layout per cluster:
   - Layout containers: `.sat-section`, `.sat-controls-row`, `.sat-grid`, `.sat-dashboard`
   - Row primitives: `.sat-row` + `.sat-label` + `.sat-value` (the 50x trio — likely a label-value row pattern)
   - Buttons: `.sat-btn` family (`-preset`, `-mode`, `-temp`, `-toggle`, `-settings-nav-btn`, `-settings-save-btn`)
   - Indicators: `.sat-indicator` + state variants, `.sat-health-*`
   - Settings group: `.sat-settings-*` (heading toggle pattern similar to existing `.sat-collapsible`)
   - Inputs: `.sat-setting-input` + state markers
   - Badges/notifications: `.sat-badge`, `.sat-badge-sim`, `.sat-feedback`

2. **Design rules using tokens** — use `var(--brand-cyan)`, `var(--brand-lightblue)`, `var(--space-*)`, `var(--fs-*)`, `var(--radius-*)` from ds-tokens.css. Reuse spacing patterns from the legacy SAT styling (TASK-472 ported them).

3. **Iterate on hardware** — write a first cut, flash, screenshot. Compare with intended visual hierarchy. Refine. This is design work, not mechanical port.

4. **Goal: functional + consistent**, not pixel-perfect against a reference (there is no reference). Match the existing SAT page tone (legacy SAT styling from sat.css gives the baseline).

## Scope

Edit `components.css` ONLY. Add a new top-level section:
```
/* =============================================================
 * 19. SAT POST-PATCH-E (new styling, no historical source)
 * ============================================================= */
```

Append after the section TASK-472 added.

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: `python tools/check_design_system_drift.py` shows drift count drops to ~17 (Bucket A only) + ~14 (Bucket C `/design.html` only) = under 35 total
- AC2: SAT dashboard page renders without unstyled blocks; all classes from the list above have a CSS rule
- AC3: Visual harmony with legacy SAT pre-Patch-E styling (TASK-472's port) — same brand cyan, same spacing rhythm, same button radius/shadow style
- AC4: Hardware screenshot review by user — matches intended SAT UX
- AC5: No console errors / 404s on SAT pages
- AC6: Dark-mode rendering verified on SAT pages (token-driven, should follow automatically)

## Out of scope

- Bucket A JS-hooks (~17 classes, no styling needed) — track in TASK-470 allowlist
- Bucket C `/design.html` reference page classes (~14, dev-tool only)
- New SAT features beyond what's already in the codebase
- Refactoring existing SAT JavaScript
<!-- SECTION:DESCRIPTION:END -->

- [ ] #1 `python tools/check_design_system_drift.py` shows drift count drops to under 35 (Bucket A JS-hooks + Bucket C design.html only)
- [ ] #2 SAT dashboard page renders without unstyled blocks; all post-Patch-E classes have CSS rules
- [ ] #3 Visual harmony with TASK-472 ported legacy SAT styling: same brand cyan, same spacing rhythm, same button shadow/radius style
- [ ] #4 Hardware screenshot review by user — matches intended SAT UX
- [ ] #5 No console errors / 404s on SAT pages
- [ ] #6 Dark-mode rendering verified on SAT pages (token-driven, should follow automatically)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Pre-conditions geland (commits 82f999ef, 9c38ba58, 32e7d907). TASK-471(cyan tokens) + TASK-473(legacy port) + TASK-474(density+scope) zijn allemaal gecommit. Token-baseline en SAT-pre-Patch-E styling zijn nu beschikbaar. Doorpakken met implementer-agent voor de ~30 SAT post-Patch-E klassen.
<!-- SECTION:NOTES:END -->
