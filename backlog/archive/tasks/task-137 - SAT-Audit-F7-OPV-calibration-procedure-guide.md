---
id: TASK-137
title: 'SAT Audit F7: OPV calibration procedure guide'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:57'
updated_date: '2026-04-09 05:27'
labels:
  - audit
  - sat
  - phase-6
  - docs
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write a technical guide for the OPV calibration procedure: when to calibrate, prerequisites, step-by-step process, expected output and troubleshooting.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Prerequisites and conditions for calibration documented
- [x] #2 Step-by-step calibration procedure documented
- [x] #3 Expected output and success criteria documented
- [x] #4 Troubleshooting section included
- [x] #5 Guide published in backlog/docs/sat-opv-calibration.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created backlog/docs/sat-opv-calibration.md. OPV = Overshoot Protection Value = max flow temp at MM=0. Calibration is a 5-phase state machine: idle -> starting -> warming (waits 5C rise, 3min timeout) -> measuring (20min, samples every 10s) -> done/failed. Total timeout 30 min. Start via MQTT sat/ovp_start, REST /api/v2/sat/settings/ovp_start, or WebUI. Result saved to settings.sat.fOvpValue and bOvpEnabled. Typical OPV range 45-60C. Documented troubleshooting for 5 failure scenarios.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created backlog/docs/sat-opv-calibration.md as a complete user-facing calibration guide. Documents: what OPV is and why it matters (proactive vs reactive mode switching), prerequisites (SAT enabled, heating demand present, 30 min required), how to start via MQTT/REST/WebUI, 6 calibration phases with state machine description, step-by-step procedure, expected output (typical OPV 45-60C, telnet confirmation message), how to cancel, how to set OPV manually, how to disable OPV, 5 troubleshooting scenarios with causes and fixes, when to re-calibrate, and full source reference with timing constants. All information verified directly against satOvpCalibrate() in SATcontrol.ino.
<!-- SECTION:FINAL_SUMMARY:END -->
