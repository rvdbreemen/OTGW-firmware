---
id: TASK-12
title: Control interval visible and configurable via all APIs
status: To Do
assignee: []
created_date: '2026-04-05 10:06'
updated_date: '2026-04-05 10:22'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The control interval (settings.sat.iControlInterval, 10-300s) is already configurable via settings but not via the SAT-specific MQTT/REST endpoints. Since the ESP sits closer to the OT bus than HA, we can adjust faster. But adjusting too fast (every second) is pointless due to heating system inertia. Make the interval visible and configurable via all channels. Document recommended values: 10-15s for radiators, 30-60s for underfloor heating (larger thermal mass). The PID sample time limit should scale with the control interval.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 REST API: GET /api/v2/sat/status includes control_interval_sec field
- [ ] #2 REST API: POST /api/v2/sat/interval with body for interval value (10-300)
- [ ] #3 MQTT subscribe: set/<nodeId>/sat/interval
- [ ] #4 MQTT publish: sat/interval
- [ ] #5 WebUI: interval field in SAT settings with tooltip about recommended values
- [ ] #6 WebUI: interval visible in Control Details section of dashboard
- [ ] #7 PID sample time limit scales: SAT_PID_SAMPLE_TIME_LIMIT = max(10, interval/3)
- [ ] #8 On interval change: CHANGE_INTERVAL_SEC(timerSATControl) applied immediately
<!-- AC:END -->
