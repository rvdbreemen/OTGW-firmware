---
id: TASK-1828
title: >-
  Monitor > Graph: temperature axis in °C, and translate the window and export
  chips
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-11T23:19:00Z'
updated_date: '2026-11-11T23:30:00Z'
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
- [x] #1 Temperature axis labels read e.g. '60 °C' (or '60°C') on the Graph, in every language
- [x] #2 Window chips and export buttons are translated in NL, DE and EL; the window choice no longer depends on the English chip text (data attribute); a test fails on the old code
- [x] #3 Browser screenshot NL and EL of the Graph toolbar
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: window chips carry data-win-ms + data-i18n (h_mon.graph_win_10m/1h/4h/24h; NL 10 min/1 uur/4 uur/24 uur, DE 10 Min./1 Std./4 Std./24 Std., EL 10 λεπ./1 ώρα/4 ώρες/24 ώρες, Greek not reviewed by Sergeant D); graphChipWindowMs() reads the attribute instead of parsing the English text; graphAxisTemp() gives '60°C'. PNG/CSV stay as format names and Auto-PNG/Auto-CSV already had data-i18n (NL/DE pack value is the same word, EL 'Αυτόματο PNG'), so no change there. KNOWN_GAPS entry '1 hour' removed from i18n-no-hardcoded (was the documented gap).

Tests: new graph-window-chips-axis.test.mjs 3 red on the old code, 3 green; i18n-no-hardcoded red (1 fail) on the old code once the '1 hour' gap entry is gone, green with the fix. Headless Chrome (fake device, 24 h synthetic history, real WS drop): NL toolbar '10 min / 1 uur / 4 uur / 24 uur', 24 h window applied, axis 70°C/35°C/0°C, markers still red@623.4 green@772.0, 0 exceptions; screenshot C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1828-24h-light-nl.png.

Extra defect found while taking the EL screenshot (in shipped beta.187, TASK-1821; 09 benched NL only): the lane labels used graph.on/graph.off, in Greek 'ενεργό'/'ανενεργό', which run past the 34-unit left margin into the flame/DHW/CH panels (C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1828-24h-light-el.png). Fix: own short keys h_mon.graph_lane_on/off (EN on/off, NL aan/uit, DE an/aus, EL ON/OFF; ON/OFF is common on Greek appliances, flagged for Sergeant D). New test in graph-panels.test.mjs: every pack's lane labels are at most 5 characters; red on the old code (2 fail), green with the fix. After: C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/f1fafa03-7939-46ad-a89a-e59de20dd2bb/scratchpad/bench/t1828b-24h-light-el.png (labels inside the margin), EL toolbar '10 λεπ. / 1 ώρα / 4 ώρες / 24 ώρες', axis °C, 0 exceptions.
<!-- SECTION:NOTES:END -->
