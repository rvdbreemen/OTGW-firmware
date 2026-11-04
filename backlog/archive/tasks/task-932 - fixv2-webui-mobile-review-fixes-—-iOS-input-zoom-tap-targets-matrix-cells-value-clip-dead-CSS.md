---
id: TASK-932
title: >-
  fix(v2-webui): mobile review fixes — iOS input-zoom, tap-targets, matrix
  cells, value-clip, dead CSS
status: Done
assignee:
  - '@claude'
created_date: '2026-06-25 05:18'
updated_date: '2026-06-26 22:30'
labels: []
dependencies: []
ordinal: 146000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Six findings from a 360px live-data mobile review of the v2 Web UI (rendered against a real OTGW on .88.16). All fixes are CSS-only in src/OTGW-firmware/data/v2.css. No JS/HTML changes. Ships in the LittleFS image; requires a prerelease bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Settings inputs + search render >=16px on mobile so iOS Safari does not auto-zoom on focus
- [x] #2 OT Support matrix cells reach >=24px tap target on mobile (WCAG 2.5.8 AA)
- [x] #3 Interactive controls connpill/seg2btn/cchip/tbtn/theme-toggle reach >=44px min-height
- [x] #4 Long settings values no longer clip on narrow phones
- [x] #5 Dead .set-cards grid-template-columns rule removed from the multicol block
- [x] #6 Tiny labels (ble-tag/mc-cell) bumped for phone readability
- [x] #7 No horizontal overflow regression at 360px; desktop layout unchanged in spirit
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
CSS-only, all in v2.css, mobile-scoped via existing @media blocks (no desktop regression, minimal surface).

#1 iOS input-zoom: add font-size:16px to .srow inputs/select + .set-search inside @media(max-width:899px) settings block.
#4 value-clip: same block — .srow{flex-wrap:wrap}; .srow inputs width:100%;max-width:none → input drops full-width under label, no clip.
#5 dead rule: remove '.set-cards{grid-template-columns:1fr}' (multicol element, grid prop is no-op).
#2 matrix tap-target: @media(max-width:719px) .matrix{grid-template-columns:repeat(8,1fr)} → ~37px cells (>=24 AA). 128 IDs → 16 rows.
#3 touch targets: @media(max-width:719px) connpill/seg2btn/cchip/tbtn/theme-toggle{min-height:44px} (were 36/34/40/40/40).
#6 tiny labels: same 719 block — .ble-tag .62→.66rem, .mc-cell .lbl .64→.68rem.

Verify: re-render @360px live, confirm >=16px inputs, 8-col matrix, no overflow. Bump prerelease (data asset). Build fs+fw, evaluator quick.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All 7 ACs in v2.css, Playwright-validated at 360px: #1 .srow text/number/password+select+.set-search font-size:16px (setSearch measured 16px, no iOS zoom); #2 .matrix repeat(8,1fr) <=719px; #3 connpill/seg2btn/cchip/tbtn/theme-toggle min-height:44px (themeToggle measured 44px); #4 .srow flex-wrap:wrap + inputs width:100% (no clip); #5 dead .set-cards grid rule removed; #6 .ble-tag .66rem/.mc-cell .lbl .68rem; #7 horizontal overflow measured 0px at 360px. CSS-only. Done.
<!-- SECTION:FINAL_SUMMARY:END -->
