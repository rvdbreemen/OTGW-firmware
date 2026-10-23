---
id: TASK-1312
title: >-
  v2 UI: one navigation concept for all pages (settings rail vs top tabs vs flat
  SAT page)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T08:17:00Z'
updated_date: '2026-10-23T12:59:00Z'
labels:
  - webui
  - ux
  - navigation
  - design
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 435000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-23 08:02 (#alpha-testing, 3 screenshots): the v2 web UI bundles functions in three different ways: Settings uses a vertical rail of groups with details on the right, other pages (Monitor, Advanced, SAT) use a row of tabs at the top, and the SAT settings are one flat page. A single, consistent navigation concept across the whole UI would improve usability; 'less is more'. Tester likes the new help tooltips. This is a UI-wide design decision for the maintainer: choose the concept (e.g. top-level pages with top tabs everywhere and grouped, collapsible sections inside; or a left rail everywhere; or tabs on top plus a rail only for long settings lists), then write it down (design note or ADR, ADR-192 v2-only UI context) and apply it page by page. Related: TASK-1310 (SAT page order), TASK-1309 (terse update page), TASK-1311 (SAT graph overlap).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer decision on the navigation concept recorded (options with consequences presented first)
- [x] #2 Concept written down as a design note or ADR, with a per-page migration list
- [x] #3 Pages migrated or follow-up tasks filed per page; before/after screenshots per migrated page
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Concept A, written as docs/design/v2-navigation-concept.md (design note; no ADR: UI convention inside ADR-192, no API/arch change).
Rules: (1) top-level pages stay the .seg row; (2) a page with sub-areas uses ONE tab component, the existing .subtabs/.subtab, and the chosen tab is remembered per page in localStorage (try/catch, like otgw-v2-advtab); (3) inside a tab/page, related fields/cards are grouped in a collapsible group = native <details class=grp><summary> (keyboard/a11y for free, no JS needed for toggling), open state remembered per group id; (4) one global save bar per page stays; (5) search opens every group with a match and closes nothing the user opened.
Per-page migration list: Settings = rail -> 11 collapsible groups on one page (SET_CATS order, count + REBOOT badge in the summary, all collapsed by default, SAT pointer card kept), deep links (BLE Assign, goToSensorRoster) open + scrollIntoView the group instead of setActiveCat; Monitor = remember tab; Advanced = already conforms (tab remembered), raw-debug <details> gets the .grp style; SAT = view buttons become .subtabs styling + SAT_L2/L3 groups collapsible -> follow-ups TASK-1310 (order) / TASK-1311 (graph overlap), so not in this task; Home view picker stays (chooses a layout, not navigation); Diagnose none.
Implementation in this task: design note + shared .grp CSS/JS helper + Settings migration + Monitor tab memory + Advanced details style. Files: data/v2.html, v2.js, v2.css (+ regenerated v2-bundle.css), tests/webui/v2-settings-sat-followups.test.mjs (rail clicks -> group summaries) + a new nav test (groups render, search opens matches, deep link opens sensors group, Monitor tab survives reload).
Verification: AC2 = the note with the migration list; AC3 = node tests red on old (rail selectors / no tab memory) then green, build.bat filesystem SUCCESS, bench .88.61 fs flash after saving /api/v2/settings, before/after screenshots desktop + 390 px of Settings and Monitor. 2-concurrent-API rule unaffected (no new fetches). Cross-line: no (v2 UI is dev-only).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 maintainer decision (session): option A, top-level pages with tabs at the top everywhere and grouped, collapsible sections inside a page; Settings loses the vertical rail and becomes one page with collapsed groups; 'less is more'. Next: write the concept down (design note or ADR) with the per-page migration list, then TASK-1310 (SAT page order) and TASK-1311 (SAT graph overlap) follow it.

2026-10-23 evidence (session 2cc0fd9b): concept docs/design/v2-navigation-concept.md with the per-page migration list (AC2). Migrated here: Settings (rail -> collapsible groups), Monitor (tab memory), Advanced (raw-debug details -> .grp); SAT follows in TASK-1310/1311, Home/Diagnose unchanged by design (AC3). Tests: v2-navigation 0 failures of 50 checks (5 of 6 runs green; the one red run came right after the implementer's Chrome exited, then 5/5 green), followups 26, sat-area-sensor-mapping 13, sat-status-pill 14, settings-help 28, settings-error-toast 4, all 0 fail. Against HEAD assets: 32 FAIL of 50, incl. the genuine old-vs-new case 'a clicked tab is stored (null)' / 'after a reload the clicked Monitor tab is active again'; Settings checks fail there only because the markup is new. evaluate --quick 0 fail 0 warn. build.bat combo SUCCESS (littlefs 14:55). Bench .88.61: settings saved (178 keys), fs flashed, 10 keys reset by the flash, restored, compare 178/178 differing 0; device serves the new v2.js/v2-bundle.css; real browser on the device: 12 groups in SET_CATS order, no rail, 0 open. Screenshots before/after desktop + 390 px (synthetic data) in docs/evidence/task-1312/. Review: 3 reviewers; fixed deep-link scroll race (pendingReveal), setPeek cleared on leaving Settings, chip opens Monitor>Connection without restoring/fetching the stored tab, refused-IP toast opens the group, docs (manual ch03/ch05, MANUAL.md); pre-existing dirty-drop on a deep link filed as TASK-1318.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
One navigation concept for the v2 UI (option A, maintainer 2026-10-23), written in docs/design/v2-navigation-concept.md with a per-page migration list. Settings: the rail is replaced by one page of native <details class=grp> groups in SET_CATS order with count and REBOOT badge, closed by default; open state is an in-memory set persisted to localStorage only on a user click (keyboard included), search opens matches transiently and clearing restores the user's set; links into Settings (BLE Assign, sensor roster) open and scroll to the group for that visit, also when GET settings is slow. Monitor remembers the clicked tab; code-chosen tabs (Connection chip) are shown without restoring or fetching the stored one. Advanced raw-debug uses .grp. Evidence: AC1 decision recorded; AC2 the design note; AC3 webui tests (navigation 50 checks green, red on HEAD incl. the tab-memory reproduction), five other webui suites green, evaluate 0 fail, build SUCCESS, bench fs flash with a settings round trip 178/178, device DOM check, before/after screenshots in docs/evidence/task-1312/. Not verified: a real phone (390 px emulated only). SAT page migration is TASK-1310/1311; the pre-existing loss of unsaved edits on a deep link is TASK-1318.
<!-- SECTION:FINAL_SUMMARY:END -->
