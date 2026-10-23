---
id: TASK-1312
title: >-
  v2 UI: one navigation concept for all pages (settings rail vs top tabs vs flat
  SAT page)
status: To Do
assignee: []
created_date: '2026-10-23T08:17:00Z'
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
- [ ] #1 Maintainer decision on the navigation concept recorded (options with consequences presented first)
- [ ] #2 Concept written down as a design note or ADR, with a per-page migration list
- [ ] #3 Pages migrated or follow-up tasks filed per page; before/after screenshots per migrated page
<!-- AC:END -->
