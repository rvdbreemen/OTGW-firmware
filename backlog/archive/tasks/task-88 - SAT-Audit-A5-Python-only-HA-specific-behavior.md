---
id: TASK-88
title: 'SAT Audit A5: Python-only HA-specific behavior'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:48'
updated_date: '2026-04-09 05:28'
labels:
  - audit
  - sat
  - phase-1
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Identify behavior in the Python integration that is inherently HA-specific and does not need to be ported (entity states, HA events, Lovelace). Document what is intentionally omitted.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 List of Python-only/HA-specific behavior documented
- [x] #2 Per item: deliberate choice or still to evaluate
- [x] #3 Clear distinction between 'not applicable' and 'missing'
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed categorization of all HA-specific behavior vs portable algorithm logic in the SAT Python integration.

Published: backlog/docs/sat-python-ha-only-features.md

Key findings:
- 16 categories identified as purely HA-specific (cannot port): config flow, entity model (ClimateEntity/SensorEntity/BinarySensorEntity/NumberEntity), service bus, event bus (EVENT_SAT_CYCLE_STARTED/ENDED, SIGNAL_PID_UPDATED), HA Store persistence, HA MQTT routing, ESPHome coordinator, Sentry error monitoring, DataUpdateCoordinator pattern, weather/sun entity reading, window sensor BinarySensorGroup, thermostat follow-mode, secondary climate cascade, DHCP/MQTT discovery handlers
- Portability table documents all algorithm features with explicit firmware equivalents
- Clear distinction applied: 'N/A' for HA-only features (intentionally not ported), 'MISSING' for portable logic not yet in firmware
- Result aligns directly with the N/A column in the feature completeness matrix (task-91)
<!-- SECTION:FINAL_SUMMARY:END -->
