---
id: TASK-3014
title: >-
  Web UI: clarify SAT multi-area vs multi-zone naming and grouping (gruijter
  suggestion)
status: To Do
assignee: []
created_date: '2026-11-21T16:14:00Z'
labels:
  - webui
  - sat
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1131000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing msg 1573733241079468133 (2026-11-21): 'multi-area' vs 'multi-zone' is confusing, and SAT/thermo-nova 'areas' correspond to our 'zones'. Suggestions: (1) rename multi-area to 'Averaged room sensors'; (2) Web UI: 'Zone count' sits under Areas while zone timeout and headroom sit under Sensors, so make two groups side by side, 'Averaged room sensors' and 'Zones', each with a one-line explanation; (3) number areas 1-4 like zones (now 0-3). Tester request: needs Robert's OK. Labels/UI text only, setting keys and MQTT/REST names unchanged unless Robert decides otherwise.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert decides which of the three suggestions to take
- [ ] #2 Web UI groups and labels changed as decided; settings keys, MQTT and REST unchanged; screenshots before/after
- [ ] #3 Docs/help text use the same names
<!-- AC:END -->
