---
id: TASK-129
title: 'SAT Audit E8: ADR for SAT MQTT topic structure'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:55'
updated_date: '2026-04-09 05:25'
labels:
  - audit
  - sat
  - phase-5
  - adr
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write an ADR documenting the MQTT topic structure for SAT: sat/ prefix, sub-topics, HA auto-discovery topics and the rationale for the chosen structure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All SAT MQTT topics documented in ADR
- [x] #2 Rationale for topic structure explained
- [x] #3 HA auto-discovery approach documented
- [x] #4 ADR published in backlog/decisions/
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created docs/adr/ADR-074-sat-mqtt-topic-structure.md documenting:
- All publish topics: <prefix>/value/<node-id>/sat/<subtopic> (flat, ~50 topics)
- All command topics: <prefix>/set/<node-id>/sat/<sub-command>
- Alternatives rejected: flat sat_ prefix, separate top-level prefix, per-category nesting
- Complete topic tables: core control, PID terms, cycle tracking, boiler status, pressure, config echo, weather (sat/weather/), BLE (ESP32 only)
- Retain policy: yes for config/stable state, no for diagnostic/transient values
- HA discovery entities: climate, sensors, binary sensors via mqttha.cfg
- Weather subtopics use sat/weather/ sub-namespace (only exception to flat structure)
<!-- SECTION:FINAL_SUMMARY:END -->
