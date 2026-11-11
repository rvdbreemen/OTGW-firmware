---
id: TASK-1828
title: >-
  Monitor > Graph: temperature axis in °C, and translate the window and export
  chips
status: To Do
assignee: []
created_date: '2026-11-11T23:19:00Z'
labels:
  - webui
  - i18n
dependencies: []
priority: low
ordinal: 913000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Findings of 09's bench of beta.187 (TASK-1821) on .61, 2026-11-12: (1) the temperature axis ticks read '60°', '30°', '0°' without 'C' (the 1.x OTGraph shows '{value}°C'; the 2.0.0 graph had the bare degree sign before TASK-1821 too: gAxHi/gAxMid/gAxLo set in renderGraph). (2) In NL (and DE/EL) the Graph window chips '10 min', '1 hour', '4 h', '24 h' and the buttons 'PNG', 'CSV', 'Auto-PNG', 'Auto-CSV' stay English: they have no data-i18n in v2.html, and the window chip handler parses the English text (/min/ and parseInt) to set graphWindowMs, so translating needs a data-attribute for the window. Evidence: 09's screenshots C2-graph-*.png and E-nl-graph.png in C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/4f6ad420-bf33-4a3a-88bc-4eb9890a2231/scratchpad/b187/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Temperature axis labels read e.g. '60 °C' (or '60°C') on the Graph, in every language
- [ ] #2 Window chips and export buttons are translated in NL, DE and EL; the window choice no longer depends on the English chip text (data attribute); a test fails on the old code
- [ ] #3 Browser screenshot NL and EL of the Graph toolbar
<!-- AC:END -->
