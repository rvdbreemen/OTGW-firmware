---
id: TASK-1297
title: >-
  Monitor graph: show the exact values at the pointer position (hover), and a
  touch equivalent on mobile
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T11:57:00Z'
updated_date: '2026-10-23T09:17:00Z'
labels:
  - webui
  - monitor
  - feature
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 422000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request gruijter 2026-10-22 08:12 (#alpha-testing, screenshot). On hover over the Monitor graph show a popup with the time and the values of the visible (switched-on) series at that x position; on touch, a tap or drag does the same. Keep it client-side (no new requests), use the existing samples and the legend's hidden-series state (gOff), textContent only, and the time axis of TASK-1267 for the x->time mapping.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Hovering the graph shows a readout with time and the values of visible series only; hidden series are omitted (bench screenshot)
- [x] #2 On a 390 px touch viewport a tap/drag shows the same readout and does not block scrolling (screenshot)
- [x] #3 A tests/webui test runs the real readout function from v2.js against sample data
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Client-side only (v2.js/v2.html/v2.css). Pure top-level function graphReadout(arr, keys, metas, off, t) -> {t, rows:[[label, text, colour]]} or null: binary search for the last record with r.t <= t in the ascending windowed array; no record, or t further than GBREAK_MS past that record's data end (r.t + (r.span||0)) -> null ('no data'); rows for every key not in off: temperatures and Dallas '21.5 °C' (1 decimal), modulation '42 %', lanes 'on'/'off', missing value '–'. Pointer handling on #gstrip (pointermove/pointerdown/pointerleave/pointercancel): clientX -> viewBox x via getBoundingClientRect (780 wide), clamp to C_X0..C_X1, t = (gLastRender - win) + (x - C_X0) / (C_X1 - C_X0) * win, i.e. the same mapping renderGraph uses; draw a vertical SVG line in #gstrip and an absolutely positioned tooltip div in .graphcard (textContent only, flips side near the right edge); hidden when the pointer leaves or a re-render moves t out of range. Touch: CSS touch-action: pan-y on #gstrip so vertical scrolling still works, tap or horizontal drag shows the readout. No new requests, no change to the ADR-184 queue. Tests: tests/webui/graph-readout.test.mjs extracts the real graphReadout (and gVal/gMeta as needed) from v2.js: nearest-before lookup, hidden series omitted, formatting per kind, gap -> null, history span honoured; red on HEAD (function missing). Bench: desktop hover screenshot, 390 px viewport touch screenshot (Playwright mobile emulation with hasTouch), scroll check (page scrolls vertically when swiping on the graph).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 11:20 evidence (bench OTGW32, %TEMP%/bench-1297): before: hovering does nothing (before-hover-desktop.png). After: desktop hover readout with time, values of visible series, '–' for missing, cursor line (after-hover-desktop.png / final-hover-desktop.png); 390 px touch: tap shows the readout for 4 s (the first version hid it at once because a tap ends with pointerleave: fixed), two-column compact tooltip clamped inside the card (final-tap-phone.png); cursor under the finger: taps at 30/50/70 % gave cursor x = tap x (diff 0.0 px) after switching the screen->viewBox mapping to getScreenCTM().inverse() (the rect-ratio mapping was off on phones because preserveAspectRatio centres the drawing); touch-action pan-y computed on #gstrip. Review fixes: readout reuses the arrays of the last render (no O(n) work per pointermove), readout refreshes after each re-render, cursor line removed from the PNG export clone, readout hidden when the Graph tab opens. tests/webui/graph-readout.test.mjs 6/6 (red on HEAD: graphReadout missing), all web UI tests 185/185, evaluate --quick 0 fail. Not verified: an actual vertical swipe scrolling the page on a real phone (only the computed touch-action). Observed during the bench work: WiFi link to the bench degraded (RSSI -75, ping up to 1 s) and hd_min_free_heap had dipped to 8296 B during repeated page loads/uploads; noted for TASK-1307.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hovering the Monitor graph now shows a readout with the time and the exact values of the visible series at that point (hidden series omitted, '–' for no value, 'no data' outside the data), with a cursor line; on touch a tap or horizontal drag shows it for 4 s and vertical swipes still scroll (touch-action: pan-y). Browser-side only, no requests. Pure graphReadout() (binary search, same gap rule as the line renderer incl. history spans) plus pointer handlers; screen->viewBox mapping via getScreenCTM so the cursor sits under the finger on phones; compact two-column tooltip on narrow screens; cursor kept out of PNG exports. Evidence per AC: AC1 desktop hover screenshot; AC2 390 px tap screenshot + cursor-under-finger measurement (0.0 px at 3 positions) + computed touch-action; AC3 graph-readout.test.mjs runs the real function (6/6, red on HEAD). Not verified: a real-phone vertical swipe.
<!-- SECTION:FINAL_SUMMARY:END -->
