---
id: TASK-5
title: Overshoot Protection Value (OPV) calibration
status: To Do
assignee: []
created_date: '2026-04-05 10:04'
updated_date: '2026-04-05 10:19'
labels:
  - sat
  - feature
  - mqtt
  - rest
  - webui
  - safety
dependencies:
  - TASK-4
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OPV is SAT's killer feature. It automatically detects a boiler's minimum output by running a calibration: send MM=0 (no modulation) + CS=high setpoint (62C radiator, 45C underfloor), wait for boiler temperature to stabilize (20+ minutes), and measure the maximum temperature. That value is the OPV and determines when PWM is necessary. On the ESP we sit directly on the OT bus and can perform calibration more accurately than from HA. The calibration runs as a state machine with phases: IDLE -> STARTING -> WARMING -> MEASURING -> DONE/FAILED. During calibration, normal SAT control loop iterations are skipped. Result is stored as a persistent setting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New enum SATCalibrationPhase: IDLE, STARTING, WARMING, MEASURING, COOLDOWN, DONE, FAILED
- [ ] #2 New state fields: state.sat.eCalibPhase, fCalibMaxTemp, fCalibStartTemp, iCalibStartMs, iCalibSamples
- [ ] #3 New setting: settings.sat.fOvpValue (default 0.0 = not calibrated)
- [ ] #4 New setting: settings.sat.bOvpEnabled (default false)
- [ ] #5 Calibration state machine in satOvpCalibrate() - called from control loop
- [ ] #6 Calibration start: send MM=0 and CS=62 (radiator) or CS=45 (underfloor), wait for flame
- [ ] #7 WARMING phase: wait until boiler temp rises above startTemp + 5C
- [ ] #8 MEASURING phase: sample boiler temp every 10s for 20 minutes, track max
- [ ] #9 DONE: store max temp as OPV in settings, send CS=0 and MM=100 to recover
- [ ] #10 FAILED: timeout after 30 min or no flame after 3 min - recover to normal operation
- [ ] #11 OPV used in control loop: if boiler temp > OPV and error < deadband, switch to PWM
- [ ] #12 REST API: GET /api/v2/sat/status includes ovp_value, ovp_enabled, calib_phase fields
- [ ] #13 REST API: POST /api/v2/sat/ovp/start - start calibration
- [ ] #14 REST API: POST /api/v2/sat/ovp/stop - cancel calibration
- [ ] #15 REST API: POST /api/v2/sat/ovp/value - manually set OPV value
- [ ] #16 MQTT subscribe: set/<nodeId>/sat/ovp_start, ovp_stop, ovp_value
- [ ] #17 MQTT publish: sat/ovp_value, sat/ovp_phase, sat/ovp_enabled
- [ ] #18 WebUI: OPV section in SAT dashboard with calibration button, current OPV value, phase indicator
- [ ] #19 WebUI: manual OPV input as alternative to calibration
- [ ] #20 HA auto-discovery: sensor entity for OPV value, binary_sensor for calibration active
- [ ] #21 Safety: calibration cancellable, timeout protection, CS=0 on failure
<!-- AC:END -->
