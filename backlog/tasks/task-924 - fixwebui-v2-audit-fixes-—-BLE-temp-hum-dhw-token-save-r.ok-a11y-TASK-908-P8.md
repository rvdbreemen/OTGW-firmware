---
id: TASK-924
title: >-
  fix(webui): v2 audit fixes — BLE temp/hum, dhw token, save r.ok, a11y
  (TASK-908 P8)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-24 11:40'
updated_date: '2026-06-24 11:41'
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
- [ ] #1 BLE temp/hum, dhw token, save r.ok, ticker aria-live, SVG labels fixed
- [ ] #2 Deferred items documented for follow-up
<!-- AC:END -->
