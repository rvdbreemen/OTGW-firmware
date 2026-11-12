---
id: TASK-1830
title: Header theme control as a Light/Dark segmented switch like the OTGW site
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:33:00Z'
updated_date: '2026-11-11T23:47:00Z'
labels:
  - webui
dependencies: []
priority: medium
ordinal: 914000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-12: the light/dark switch on https://rvdbreemen.github.io/OTGW-site/ is a neat solution; use it in 2.0.0. Site (assets/site.css .switch, site.js): role=group with two buttons '☀ Light' and '☾ Dark' (data-theme-set, aria-pressed), the active one in the accent colour, labels hidden below 1100 px. 2.0.0 now has one action button #themeToggle ('🌙 Dark' / '☀ Light', TASK-1692 D1: action label, no aria-pressed). This replaces D1 with the other valid WAI-ARIA pattern (two buttons with fixed labels, aria-pressed state). Storage stays localStorage otgw-theme and the pre-paint script.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Header shows a Light/Dark segmented switch (☀ Light | ☾ Dark) in one outlined group; the active theme's button is highlighted and has aria-pressed=true, the other false; clicking sets the theme and stores it (otgw-theme)
- [x] #2 Labels translated in EN/NL/DE/EL; below 1100 px only the icons show; no horizontal scroll at 390 px
- [x] #3 theme-toggle-labels test updated for the new pattern and red on the old code; browser screenshots light and dark
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: #themeSwitch role=group (aria-label core.theme_group) with two buttons data-theme-set light/dark, fixed labels (☀ + core.theme_light, ☾ + core.theme_dark; the icons moved out of the translations), title core.theme_to_light/to_dark, aria-pressed set by applyTheme(); click sets and stores otgw-theme; CSS copies the site's .switch (outlined group, active = accent background, labels hidden below 1100 px). theme-toggle-labels D1 rewritten for the new pattern (deliberate contract change, TASK-1692 D1 replaced); obsolete i18n-no-hardcoded allow entries for themeToggle removed. Greek 'Φωτεινό'/'Σκούρο'/'Θέμα' already existed or are new for theme_group: flagged for Sergeant D.

Evidence: theme-toggle-labels.test.mjs on the old code 1 fail (no #themeSwitch), on the fix 4 pass (fixed labels, aria-pressed follows, click stores, survives reload); i18n-no-hardcoded 3, i18n-lang 6, v2-undefined-names 2. Headless Chrome: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1830-light-desktop.png, t1830-dark-desktop.png, t1830-dark-phone.png (icons only at 390 px, no horizontal scroll), t1830-dark-desktop-el.png (Φωτεινό/Σκούρο), 0 exceptions.
<!-- SECTION:NOTES:END -->
