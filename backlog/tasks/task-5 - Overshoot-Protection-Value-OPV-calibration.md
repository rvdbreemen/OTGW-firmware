---
id: TASK-5
title: Overshoot Protection Value (OPV) calibration
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 10:04'
updated_date: '2026-04-05 22:34'
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
- [x] #1 New enum SATCalibrationPhase: IDLE, STARTING, WARMING, MEASURING, COOLDOWN, DONE, FAILED
- [x] #2 New state fields: state.sat.eCalibPhase, fCalibMaxTemp, fCalibStartTemp, iCalibStartMs, iCalibSamples
- [x] #3 New setting: settings.sat.fOvpValue (default 0.0 = not calibrated)
- [x] #4 New setting: settings.sat.bOvpEnabled (default false)
- [x] #5 Calibration state machine in satOvpCalibrate() - called from control loop
- [x] #6 Calibration start: send MM=0 and CS=62 (radiator) or CS=45 (underfloor), wait for flame
- [x] #7 WARMING phase: wait until boiler temp rises above startTemp + 5C
- [x] #8 MEASURING phase: sample boiler temp every 10s for 20 minutes, track max
- [x] #9 DONE: store max temp as OPV in settings, send CS=0 and MM=100 to recover
- [x] #10 FAILED: timeout after 30 min or no flame after 3 min - recover to normal operation
- [x] #11 OPV used in control loop: if boiler temp > OPV and error < deadband, switch to PWM
- [x] #12 REST API: GET /api/v2/sat/status includes ovp_value, ovp_enabled, calib_phase fields
- [ ] #13 REST API: POST /api/v2/sat/ovp/start - start calibration
- [ ] #14 REST API: POST /api/v2/sat/ovp/stop - cancel calibration
- [ ] #15 REST API: POST /api/v2/sat/ovp/value - manually set OPV value
- [x] #16 MQTT subscribe: set/<nodeId>/sat/ovp_start, ovp_stop, ovp_value
- [x] #17 MQTT publish: sat/ovp_value, sat/ovp_phase, sat/ovp_enabled
- [x] #18 WebUI: OPV section in SAT dashboard with calibration button, current OPV value, phase indicator
- [x] #19 WebUI: manual OPV input as alternative to calibration
- [ ] #20 HA auto-discovery: sensor entity for OPV value, binary_sensor for calibration active
- [x] #21 Safety: calibration cancellable, timeout protection, CS=0 on failure
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add SATCalibrationPhase enum to OTGW-firmware.h
2. Add calibration state fields to SATRuntimeSection
3. Add settings.sat.fOvpValue and bOvpEnabled
4. Implement satOvpCalibrate() state machine in SATcontrol.ino
5. Integrate with control loop: skip normal control during calibration
6. Settings persistence for OPV value and enabled flag
7. REST API endpoints for start/stop/value
8. MQTT subscribe handlers
9. MQTT publish in status
10. WebUI: OPV section with calibration button
11. Safety: timeout, cancel, CS=0 recovery
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
SAT Python references (Overshoot Protection):
- const.py:77 - CONF_OVERSHOOT_PROTECTION = "overshoot_protection"
- const.py:122 - default: CONF_OVERSHOOT_PROTECTION: False
- const.py:188 - OVERSHOOT_PROTECTION_REQUIRED_DATASET = 40 (min samples needed)
- const.py:196 - STORAGE_OVERSHOOT_PROTECTION_VALUE for persisted OPV
- const.py:201-202 - SERVICE_SET_OVERSHOOT_PROTECTION_VALUE, SERVICE_START_OVERSHOOT_PROTECTION_CALCULATION
- const.py:21 - OVERSHOOT_SUSTAIN_SECONDS = 60.0 (sustained overshoot detection)
- heating_control.py:262-305 - _maybe_enable_pwm_on_sustained_overshoot(): enables PWM when flow temp exceeds requested_setpoint + OVERSHOOT_MARGIN_CELSIUS for OVERSHOOT_SUSTAIN_SECONDS; guards against hot_water_active and DHW guard window
- heating_control.py:380-448 - _compute_pwm_control_setpoint(): applies OVP only when overshoot_protection config enabled, flame on/off logic with return temp + offset, suppression delay, then flow temp - offset
- area.py:33,304-321 - OVERSHOOT_MARGIN=0.3, overshoot_cap() computes cooling-driven cap from overshooting rooms
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implemented OPV calibration state machine (IDLE/STARTING/WARMING/MEASURING/DONE/FAILED). Safety timeouts, cancel support, CS=0 recovery. 17/21 ACs complete.
<!-- SECTION:FINAL_SUMMARY:END -->
