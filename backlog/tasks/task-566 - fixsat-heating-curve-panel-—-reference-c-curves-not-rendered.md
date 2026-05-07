---
id: TASK-566
title: 'fix(sat): heating-curve panel — reference c-curves not rendered'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 19:20'
updated_date: '2026-05-07 21:56'
labels:
  - sat
  - webui
  - charts
  - 2.0.0
dependencies: []
priority: high
ordinal: 29000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On alpha.8 SergeantD's screenshot shows the SAT Heating Curve panel partially renders: grid + axes + active orange dot at ~(21°, 27°) all draw correctly. But the 10 grey reference curves c=0.5..5.0 are missing. echarts is initialised (Temperature History panel works fine), so this is no longer a chart-init bug — it's a series-construction bug in buildCurveOption() at data/sat.js:470. Either the refCoeffs loop isn't running, the result isn't being added to the series array, or the series array is being filtered/overwritten. Narrowed diagnosis supersedes TASK-544 AC #1 (which assumed the chart was completely blank). Field evidence: alpha.8 screenshot 2026-05-07.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 buildCurveOption() in data/sat.js:470 is read end-to-end and the precise reason refCoeffs (c=0.5..5.0) are not in the rendered series array is identified in the implementation notes
- [x] #2 The 10 grey reference curves render in the Heating Curve (Stooklijn) panel on a fresh build with SAT enabled, regardless of live data availability (cold device, no boiler attached)
- [x] #3 The active coefficient curve (orange) renders alongside the grey references; the active dot continues to render at the current outside / flow point as it does today on alpha.8
- [x] #4 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [x] #5 python3 evaluate.py --quick — no new failures
- [ ] #6 Field validation on alpha.12+: SergeantD or another tester confirms the 10 grey reference curves render on the SAT page (deferred — hardware screenshot)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. inspect buildCurveOption; 2. fix c-curves rendering with bright palette; 3. ship; 4/5/6 verify.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Design constraint from user (2026-05-07):**

The reference c-curves must be rendered in **bright, colorful, distinct lines** — explicitly NOT:
- The same colour as the grid lines (avoid blending into the chart background — was #bbb in light theme / #555 in dark theme per the prior implementation reference)
- The same colour as the active "dot" (currently orange #ff6600)

This means the prior "grey reference curves" approach is rejected. The fix should pick a palette where the 10 curves are individually visible against the dark theme background and against the chart grid, and clearly distinguishable from the active orange dot. Reasonable options to evaluate during implementation:
- A 10-step distinct-hue palette (one colour per c-coefficient)
- A 10-step colour ramp (e.g., blue→cyan→green) where each curve is clearly distinct but the family is visually grouped
- A solid bright accent colour (e.g., #4DA6FF blue or #80CBC4 teal) shared across all 10 curves but with subtle line-width / dash variation

Pick one during implementation and document the choice in the commit message. The end-user requirement is "I can see all 10 curves clearly in expert mode" — pick the palette that achieves that without making the chart busy.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in alpha.13. Root cause: echarts xAxis.type='value' on the heating-curve chart required [x,y] pair input but the c-curve series were emitting scalar y values, so echarts treated each curve as a single-point dataset and drew nothing. Fix: convert refCoeffs to [x,y] pair arrays + apply a bright reference palette so the 10 curves are visible against the dark theme. Active orange curve and dot continue to render alongside. Build and evaluator green. AC #6 (SergeantD or another tester confirms 10 grey reference curves render with hardware screenshot) gated on field validation.
<!-- SECTION:FINAL_SUMMARY:END -->
