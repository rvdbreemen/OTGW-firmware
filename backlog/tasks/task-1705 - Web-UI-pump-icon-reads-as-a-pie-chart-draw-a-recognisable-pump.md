---
id: TASK-1705
title: Web UI pump icon reads as a pie chart; draw a recognisable pump
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T17:08:00Z'
updated_date: '2026-11-08T09:03:00Z'
labels:
  - webui
  - field-report
dependencies: []
priority: low
ordinal: 818000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-07 14:45 UTC: the pump icon looks like a manic mushroom (a blue disc with a cut-out segment). Replace it with a recognisable circulation pump symbol in the v2 UI design system.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Pump icon replaced by a recognisable pump symbol in light and dark theme
- [x] #2 Before/after screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (lane-1705 worktree, uncommitted): v2.html #pumpBlades was three arc wedges (pie chart look). Now one triangle M437,340 L459.5,327 L459.5,353 Z inscribed in the r=16 pump circle at (452,340), apex pointing left along the return flow (same direction as .arrowCold). v2.css: .pump-dir fill/stroke var(--muted) idle, var(--accent) under svg.ch-on with an opacity pulse (pumpPulse); the rotate spin and @keyframes spin are removed (no other user of spin in data/). #pumpBlades id and ch-on hook kept. Visible behaviour change: idle pump is grey instead of always accent.

Test: tests/test_webui_pump_icon.py reads the real v2.html/v2.css (OTGW_V2_HTML/OTGW_V2_CSS override for old-vs-fix). Fix: python tests/test_webui_pump_icon.py -> Ran 5, OK. Old (git show lane-1701 copies): Ran 5, FAILED (failures=4: P1 triangle, P2 inscribed, P3 apex follows flow, P4 not rotated). Mutant apex pointing right: FAILED (failures=1, P3).

Gates: python evaluate.py --quick -> Passed 84, Warnings 0, Failed 0 (Design-System Class Drift clean). pytest tests/test_webui_pages.py + pump test: 19 passed. node --test tests/webui/*.test.mjs: 237 tests, fails varied 5 then 2 under the parallel run (graph-room-dropout, sat-beta-notice, 60-105 s timeouts on a loaded machine); those two run alone: 2 pass 0 fail. No webui test references pumpBlades or pump-body.

Screenshots (headless Edge, local file render, synthetic page without device): C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/74b8fc81-f282-4eda-bcab-6276a4a766f4/scratchpad/tjfs-1107/pump-before-light.png, pump-before-dark.png, pump-after-light.png, pump-after-dark.png (standalone pump group with real ds-tokens.css and v2.css, 24 px and 96 px frames, idle and ch-on); C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/74b8fc81-f282-4eda-bcab-6276a4a766f4/scratchpad/tjfs-1107/page-before-dark.png and page-after-dark.png (full real v2.html, dark theme because headless Edge followed the system theme). Recognisability (AC1) needs a human look by Robert or tjfs; not ticked.

Robert 2026-11-08 after the before/after screenshots (light and dark): 'Ik vind het pomp icoon goed' - the symbol is recognisable, grey idle and the pulse are accepted. Lead integration on wt-1678 (origin/dev 224a1f7ae + TASK-1662 + TASK-1682): test_webui_pump_icon fix 5/5 OK, old v2.html/v2.css (HEAD copy) FAILED; evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: pump symbol (triangle in the circle along the return flow), grey idle, accent + pulse while CH runs; Robert approved the screenshots 2026-11-08; tests/test_webui_pump_icon.py old 4 fail, new 5/5. AC2: before/after light+dark screenshots (scratchpad tjfs-1107) plus device screenshot on beta.141 (.61). Pushed 9efbfa941 (beta.139), #alpha-testing 1568913428758798336 (zip beta.141+963b68d covers 137-141).
<!-- SECTION:FINAL_SUMMARY:END -->
