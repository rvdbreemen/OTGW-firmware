---
id: TASK-1830
title: Header theme control as a Light/Dark segmented switch like the OTGW site
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T23:33:00Z'
updated_date: '2026-11-12T02:37:00Z'
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

Integrated and built: rebased on ac5967ad2, one bump per task (1823 beta.191, 1828 192, 1830 193, 1832 194, 1831 195, 1833 196, 1845 197) + test commit 4cafc97eb; full run_tests.py 208/210 (test_override_reply fixed in 4cafc97eb, test_stream_real_cbuf -j4-only, unrelated); evaluate --quick 93 checks 0 FAIL; build.bat esp32-combo 'Build completed successfully', build/OTGW-firmware-esp32-combo-2.0.0-beta.197+4cafc97-flash.zip (6158.3 KB). Bench requested from 09 via the coordinator.

Bench (otgw-bench-tester 09, .61, beta.197+4cafc97 vs 190+1a4c0a4): ota_flash app+fs, githash 4cafc97, bootcount 392->394, health 200, MQTT connected, crashlog none, settings 164 unchanged; restored afterwards (settings diff 0, bootcount 395). Pushed ac5967ad2..4cafc97eb, zip posted (Discord 1570265549747781763). TASK-1830 PASS: segmented ☀Light | ☾Dark, active segment accent (#00bffe dark, rgb(2,136,160) light), aria-pressed follows, survives reload, icons only at 1000 px, labels at 1280 px, EL Φωτεινό/Σκούρο.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1 segmented switch with aria-pressed, click stores: theme-toggle-labels red on old/green, 09 PASS. AC2 translated, icons below 1100 px, no horizontal scroll: screenshots + 09. AC3 test + screenshots light/dark. Commit d48eb2b0c (beta.193), pushed, posted.
<!-- SECTION:FINAL_SUMMARY:END -->
