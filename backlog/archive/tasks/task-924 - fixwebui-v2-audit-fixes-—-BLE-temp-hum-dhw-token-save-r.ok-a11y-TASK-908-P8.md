---
id: TASK-924
title: >-
  fix(webui): v2 audit fixes — BLE temp/hum, dhw token, save r.ok, a11y
  (TASK-908 P8)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-24 11:40'
updated_date: '2026-06-26 22:30'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 140000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Deep audit (stress+MQTT+REST+UI) findings. FIXED: (1) BLE roster temp/hum never displayed — v2.js read s.temperature/humidity but firmware emits temp/hum; (2) --dhw token defined-but-unused, .chip-dhw hardcoded #ff9b3c -> var(--dhw); (3) saveSettings reported success on HTTP error (no r.ok check) -> throw on !r.ok; (4) a11y: #cTicker aria-live, strip-chart SVG aria-labels; (5) trimmed unfulfilled OT-Support 'carries the spec' caption. DEFERRED (documented, separate task): unwired mockup controls (Concept-A heat-pump toggle, Concept-B steppers, Stats column sort, Log Download, Graph time-range chips, Connection per-link detail rows, Support hover tooltip); v2.js has no REST seed/poll fallback (blank if /ws down); doc drifts (stale ArduinoJson comments in restAPI, ADR-124->140 stale comments+MEMORY note, otgw_connected non-retained vs docs/api/MQTT.md says retained); stale SAT json-golden oracle (ovp_*/heating_source).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 BLE temp/hum, dhw token, save r.ok, ticker aria-live, SVG labels fixed
- [x] #2 Deferred items documented for follow-up
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
All AC#1 fixes verified present in shipped v2 assets: BLE roster reads s.temp/s.hum with s.temperature/s.humidity fallback (v2.js:1448); saveSettings throws on !r.ok (v2.js:1315); .chip-dhw uses var(--dhw) (v2.css); #cTicker + #monLog role=log aria-live=polite (v2.html:305/334); strip/gstrip/connMap/schem/dial SVGs carry aria-label. AC#2 deferred items documented in description (most since landed via TASK-925/933). Self-verifiable, no hardware AC — Done.
<!-- SECTION:FINAL_SUMMARY:END -->
