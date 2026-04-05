---
id: TASK-12
title: Control interval visible and configurable via all APIs
status: To Do
assignee: []
created_date: '2026-04-05 10:06'
updated_date: '2026-04-05 21:42'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT Python references (Control interval):
- pid.py:20 - PID_UPDATE_INTERVAL = 60 (seconds, the core PID cycle time)
- pid.py:191 - integral accumulation uses PID_UPDATE_INTERVAL: _integral += ki * error * PID_UPDATE_INTERVAL
- pid.py:217 - derivative skips update if delta_time <= PID_UPDATE_INTERVAL
- pid.py:228 - low-pass filter alpha = delta_time / (PID_UPDATE_INTERVAL + delta_time)
- climate.py:29 - imports async_track_time_interval from HA helpers
- climate.py:40 - imports PID_UPDATE_INTERVAL from pid module
- climate.py:707 - async_track_time_interval schedules control_heating_loop
- climate.py:713-714 - async_track_time_interval schedules control_pid at PID_UPDATE_INTERVAL cadence
- area.py:15,21 - imports async_track_time_interval + PID_UPDATE_INTERVAL
- area.py:188-189 - per-area PID also uses async_track_time_interval(hass, control_pid, timedelta(seconds=PID_UPDATE_INTERVAL))
<!-- SECTION:NOTES:END -->
