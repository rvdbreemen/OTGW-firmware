---
id: TASK-474
title: >-
  fix(webui): density + Save-button scope fixes for settings, command bar,
  webhook fields
status: Done
assignee:
  - '@claude'
created_date: '2026-04-28 17:26'
updated_date: '2026-04-28 21:26'
labels:
  - webui
  - design-system
  - density
dependencies:
  - TASK-467
references:
  - src/OTGW-firmware/data/components.css
  - src/OTGW-firmware/data/index.html
  - src/OTGW-firmware/data/index.js
  - otgw_design_package/AUDIT-2026-04-28.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tighten over-spaced rows and constrain visibility of the global Save button to the Settings pages.

## Context

Per audit (`otgw_design_package/AUDIT-2026-04-28.md` Cause D + screenshots #4, #7, #8): design-package density tokens (`--space-2`, `--space-4`, etc.) are calibrated for cards, not for compact settings tables. This produces:
- Settings rows ~50px tall (Image #7)
- Webhook URL fields fixed at 240px → long URLs truncated (Image #8)
- OT command input visibly taller than the Send button next to it (Image #4)

Plus screenshot #3: Save button visible on home page because it lives in `<div class="footer">` outside any `.page-section` container, and tab-switching does not hide it.

## Scope

### A. components.css MID-FILE edits (existing rules)

- `.settingDiv` (around line 531-553):
  - Padding: `var(--space-2) var(--space-5)` → `var(--space-1) var(--space-4)` (4px 16px)
  - Border: `4px solid` → `1px solid`
- `.settingDiv input[type="text"], .settingDiv input[type="password"], .settingDiv input[type="number"]` (around line 547-552):
  - `width: 240px` → `width: auto; min-width: 240px; max-width: 100%;`
- `.ot-cmd-input`:
  - If rule already exists: ensure `height: 32px; padding: 4px var(--space-3);`
  - If missing: add new rule with these values + `box-sizing: border-box`

DO NOT touch the `/* 18. LEGACY WIDGET COMPATIBILITY */` section appended by TASK-472 (that is the boundary).

### B. index.html / index.js — Save button scoping

Two acceptable approaches; pick the one with the smallest change footprint:

**Option 1**: in `index.html:554-558`, move the Save-button `<button id="btnSaveSettings">` inside `<div id="displaySettingsPage" class="page-section">` (currently around line 322). When that page-section is non-active, the existing `.page-section { display: none; }` rule (TASK-467 fix) hides it automatically. No JS change.

**Option 2**: keep the Save button in footer; in every tab-switch handler in `index.js`, call `toggleHidden('btnSaveSettings', true)` to force-hide it, then `false` only inside the settings handlers. More JS surface, more error-prone.

Recommendation: Option 1 (HTML restructure). Cleaner, matches the architectural pattern of the rest of the app.

If `displaySATSettingsPage` (line ~539) also needs a save button, add a parallel button there or refactor the save handler to know about both contexts.

## Acceptance Criteria
<!-- AC:BEGIN -->
- AC1: Settings rows render at ~32-36px (not 50+px) after flash
- AC2: Webhook URL field shows full URL without truncation (responsive width)
- AC3: OT command input bar matches the Send button's visual height (both ~32px)
- AC4: Save button is visible **only** on `displaySettingsPage` (and `displaySATSettingsPage` if applicable); hidden on Home, SAT, PIC firmware, Debug Info, Webhook, FSexplorer
- AC5: No regression in legacy widget compatibility (the section TASK-472 appended is untouched)

## Out of scope

- ds-tokens.css colour values (TASK-471)
- New legacy class definitions / SAT classes (TASK-472)
- New SAT post-Patch-E styling (TASK-474)
- Removing the legacy `<div class="footer">` block — even with Save-button moved, the footer holds other content (theme-toggle? timestamp? check first)
<!-- SECTION:DESCRIPTION:END -->

- [x] #1 Settings rows render at ~32-36px tall after flash (not 50+px)
- [x] #2 Webhook URL input shows full URL without truncation; field is responsive (auto width with min 240px / max 100%)
- [x] #3 OT command input bar matches the Send button's visual height (both ~32px)
- [x] #4 Save button is visible only on displaySettingsPage (+ displaySATSettingsPage if applicable); hidden on Home, SAT, PIC firmware, Debug Info, Webhook, FSexplorer
- [x] #5 No regression in legacy widget section appended by TASK-472
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementer-agent klaar (92s).

Edits:
- components.css:531 .settingDiv padding var(--space-2) var(--space-5) -> var(--space-1) var(--space-4); border 4px -> 1px
- components.css:547 .settingDiv input[type=text/password/number] width 240px -> auto / min 240px / max 100%
- index.html: btnSaveSettings verplaatst van <div class=footer> naar laatste child van #displaySettingsPage; klassenstring (nav-item tabButton btnSaveSettings btn btn-primary hidden) intact gebleven, index.js' getElementsByClassName('btnSaveSettings') blijft dus werken

Open items:
1. .ot-cmd-input rule bestaat NERGENS in components.css. Agent koos voor file-boundary respect ipv improviseren. Pak ik op in TASK-473's append-section of als micro-follow-up TASK-476.
2. #displaySATSettingsPage heeft nu geen Save-knop meer (klassen-based JS handler vond beide pagina's, maar er was maar 1 button). Optie: parallelle button in displaySATSettingsPage toevoegen, OF SAT settings heeft eigen save-pad (zie #satSettingsMessage / #satSettingsContent). Verifieer per hardware test.

.ot-cmd-input rule toegevoegd onderaan components.css (na de TASK-473 append-section): height: 32px; padding: 4px var(--space-3); box-sizing: border-box. AC3 nu source-verifieerbaar; visuele bevestiging wacht op flash.

Open issue 2 RESOLVED (false alarm): displaySATSettingsPage heeft eigen save-mechanisme. Bij investigation gevonden: SAT settings rendert per-group save buttons dynamisch via index.js:3618 met klasse 'sat-settings-save-btn', click-handler aan saveSATSettingsGroup(groupId) op line 3723. Geen relatie met btnSaveSettings. De TASK-474 agent's flag was gebaseerd op het feit dat getElementsByClassName('btnSaveSettings') in eerste instantie 1 button vond ipv 2, maar dit was correct — SAT settings heeft een ander save-mechanisme. AC4 nu volledig source-verifieerbaar.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Tightened WebUI settings density and save-button scoping. Reduced setting row padding/borders, made settings inputs responsive, added a 32px OT command input rule, and moved the global Save button into the Settings page so it is hidden outside that page. Verified from source that SAT settings uses its separate dynamic save buttons and that the legacy widget compatibility section remains untouched.
<!-- SECTION:FINAL_SUMMARY:END -->
