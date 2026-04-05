---
id: TASK-6
title: PID integral and derivative persistence to LittleFS
status: To Do
assignee: []
created_date: '2026-04-05 10:04'
updated_date: '2026-04-05 10:19'
labels:
  - sat
  - feature
  - persistence
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT Python saves PID state in HA store so after restart the PID doesn't need to re-learn. On the ESP we currently lose integral and derivative state on every reboot, causing 5-15 minutes of suboptimal control. Save PID state periodically to LittleFS (every 5 minutes, and on clean shutdown). On boot: load saved state if not older than 1 hour. Use a separate small file /pid_state.json. Note: don't write too often due to flash wear - 5 min interval is safe (288 writes/day, flash rated for 100K+ cycles per sector).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New file /pid_state.json with fields: integral, lastError, rawDerivative, lastRoomTemp, timestamp
- [ ] #2 satPidSave() function: writes current PID state to LittleFS as JSON
- [ ] #3 satPidLoad() function: reads PID state from LittleFS, validates timestamp (max 1 hour old)
- [ ] #4 Timer DECLARE_TIMER_SEC(timerPidSave, 300) for periodic saving (every 5 min)
- [ ] #5 At initSAT(): call satPidLoad() to restore state
- [ ] #6 At satDisable(): call satPidSave() for clean shutdown
- [ ] #7 At handleReboot()/ESP.restart(): call satPidSave()
- [ ] #8 Validation: if loaded integral > 20.0 or < -20.0, reset to 0 (corruption protection)
- [ ] #9 REST API: GET /api/v2/sat/status includes pid_persisted boolean and pid_last_save_ms
- [ ] #10 WebUI: indicator in PID section whether state was restored after boot
- [ ] #11 MQTT publish: sat/pid_persisted (true/false)
<!-- AC:END -->
