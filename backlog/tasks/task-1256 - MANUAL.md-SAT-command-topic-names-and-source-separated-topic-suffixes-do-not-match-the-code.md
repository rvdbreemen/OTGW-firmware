---
id: TASK-1256
title: >-
  MANUAL.md: SAT command topic names and source-separated topic suffixes do not
  match the code
status: To Do
assignee: []
created_date: '2026-10-19 17:32'
labels:
  - docs
  - mqtt
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 388000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by gruijter 2026-10-19, verified in code. SAT command table (docs/MANUAL.md ~3161-3165) lists sat/externaltemp, sat/externaloutdoor, sat/enable, sat/mode; kSatMqttCmds[] (MQTTstuff.ino ~658-665) accepts sat/indoor_temp, sat/outdoor_temp, sat/enabled, sat/control_mode (MANUAL ~863-866 already has them right). Source-separated topics (MANUAL ~495, ~3062-3064) say <label>/thermostat, /boiler, /gateway; publishToSourceTopic() publishes <label>_thermostat and <label>_boiler, no gateway variant.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 MANUAL.md SAT command table matches kSatMqttCmds[]
- [ ] #2 MANUAL.md source-separated topic text matches publishToSourceTopic() (underscore suffix, no gateway)
<!-- AC:END -->
