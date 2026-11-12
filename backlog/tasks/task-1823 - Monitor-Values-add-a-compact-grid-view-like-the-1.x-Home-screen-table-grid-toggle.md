---
id: TASK-1823
title: >-
  Monitor > Values: add a compact grid view like the 1.x Home screen (table/grid
  toggle)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T21:22:00Z'
updated_date: '2026-11-12T00:26:00Z'
labels:
  - webui
dependencies: []
priority: medium
ordinal: 909000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-11 chose 'grid view next to the table'. 1.x Home (OTGWELGA .16) shows all otmonitor fields in a dense 5-column grid with on/off boxes and value+unit, everything at a glance. 2.0.0 Values keeps its filterable table; a toggle switches to the grid. Same data (/api/v2/otgw/otmonitor), translated names (TASK-1820 pattern: names follow a language switch). Toggle choice per browser (localStorage).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Values has a Table/Grid toggle; the grid shows every otmonitor field in columns: booleans as filled/empty box, numbers with unit
- [x] #2 Grid names follow a live language switch (EN/NL/DE/EL); host or browser test
- [x] #3 Browser screenshot of the grid next to the 1.x Home screen
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented (parked): Table/Grid chips in the Values toolbar (localStorage otgw-v2-valview, default table); #valGrid uses CSS column-width 300px like the 1.x .otmontable (350px); cell = name | value | unit, on/off as a box (.vsq on/alarm for fault flags), no-data fields left out as in 1.x, sensorsimulation hidden; rebuilt on every renderValues so names follow a language switch and the filter applies. Keys h_mon.val_view_table/val_view_grid in EN/NL/DE/EL (Πίνακας/Πλέγμα not yet reviewed by Sergeant D). values-names-follow-language.test loads renderValGrid now (renderValues calls it).

Evidence: tests/webui/values-grid-view.test.mjs 4 red on the old code, 4 green; values-names-follow-language 2 green, i18n-lang 6, i18n-no-hardcoded 3, v2-undefined-names 2, v2-nested-helper-scope 2. Browser (headless Chrome, real v2 page, fake device answering with OTGWELGA's real /api/v2/otgw/otmonitor of 2026-11-11): 31 cells, table hidden, EN 'Flame Status' -> NL 'Vlamstatus' live, 390 px wide no horizontal scroll, 0 exceptions; screenshots C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/grid-en-light.png, grid-nl-light.png, grid-nl-phone.png.

Parked per coordinator (Robert: finish first): the work is a patch, .git/wip-task-1823.patch (copy C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/wip-task-1823.patch), removed from the tree so the 186/187 build stays clean; git apply --check confirms it re-applies on HEAD. Commit with its own normal bump after d3's beta.188, full tests/webui run first.

Defect found before the batch build: evaluate.py --quick FAIL 'Design-System Class drift: table, grid' from setValView() 'classList.toggle("on", valView === "table")' (the ADR-091 scanner reads every literal in a classList call as a class). Fix at the rebase: compute the boolean first (var g = valView === 'grid'; toggle('on', !g) / toggle('on', g)). My earlier evaluator run for 1823 predated that code; the full run on all local commits caught it.
<!-- SECTION:NOTES:END -->
