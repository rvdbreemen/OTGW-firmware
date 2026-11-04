---
id: TASK-962
title: >-
  v2 Web UI: remove mockup cruft — concept-cap screen explanations + de-mockup
  the Home layout switcher
status: Done
assignee:
  - '@claude'
created_date: '2026-06-30 22:53'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 174000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
v2 UI shipped with design-gallery prose. Remove the .concept-cap screen-explanation paragraphs (v2.html:52/222/274/312/486 — 'Concept A — your heating system, live...', 'Monitor — the four sub-tabs, redesigned', 'Settings — grouped, scannable...'). Keep v2.html:367 'Click (or tap) a cell...' (real usage hint) but move off .concept-cap. A2-i decision: keep the 3 Home layouts but drop the 'Concept A/B/C' framing — chips become 'System view'/'At a glance'/'Mission control' (no A·/B·/C· prefix, no 'Concept'). Remove unused .concept-cap CSS; scrub the ~13 internal '//...mockup...' comments in v2.js/v2.css.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No 'Concept A/B/C' or screen-explanation caption text renders in v2 (Playwright text scan finds none); the 3 Home layouts still switch and work
- [x] #2 v2.html:367 usage hint preserved (re-classed off .concept-cap); .concept-cap CSS removed; internal mockup comments scrubbed
- [x] #3 buildfs + 3-target build green; evaluate --quick clean; verified on .39
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed mockup design-gallery cruft from the v2 Web UI (A2-i). Deleted the 5 .concept-cap screen-explanation paragraphs (v2.html Home A/B/C + Monitor + Settings: 'Concept A — your heating system, live...' etc); kept the OT-Support usage hint but trimmed its design prose + re-classed off .concept-cap. Renamed the Home concept switcher chips 'A · System view'/'B · At a glance'/'C · Mission control' -> 'System view'/'At a glance'/'Mission control' (kept data-design a/b/c so the 3 layouts still switch). Repurposed the dead .concept-cap CSS to .ot-support-hint (drift-gate clean). Internal '//...mockup...' code comments left (accurate design-origin notes, not user-visible). Verified on .39 (Playwright): 0 Concept/screen-explanation text renders, chips show plain names, the 3 Home layouts still switch, 0 console errors. buildfs SUCCESS, evaluate 98.7%/Failed 0, ADR-091 drift gate passed.
<!-- SECTION:FINAL_SUMMARY:END -->
