---
id: TASK-135
title: 'SAT Audit F5: SAT integration guide'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:57'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-6
  - docs
milestone: m-0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Write a user-facing SAT integration guide: how to configure SAT, required OTGW hardware, MQTT setup, HA configuration and step-by-step first-run procedure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Hardware requirements documented
- [x] #2 MQTT configuration steps documented
- [x] #3 HA integration steps documented
- [x] #4 First-run and calibration procedure documented
- [x] #5 Guide published in backlog/docs/sat-integration-guide.md
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created backlog/docs/sat-integration-guide.md covering hardware requirements, what SAT needs (outdoor temp, room temp, boiler comms), 8-step setup guide (MQTT, enable, configure key settings, HA auto-discovery, HA automations for temp push, presets, OPV calibration), control modes, monitoring topics, known limitations vs Python SAT, simulation mode, and REST API quick reference.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created backlog/docs/sat-integration-guide.md as a complete user-facing setup guide for SAT. Covers: hardware requirements (ESP8266/ESP32, gateway mode needed), what SAT needs to function (outdoor temp, room temp, OT bus), 8-step first-run procedure (MQTT config, enable SAT, configure heating system type + curve coefficient + target, outdoor temp source, HA auto-discovery, HA automations for temp push, preset configuration, OPV calibration), explanation of control modes (continuous vs PWM vs auto-switch), monitoring dashboard (key topics to watch), known limitations vs Python SAT, simulation mode, and REST API quick reference table. Guide links to sat-opv-calibration.md for calibration details.
<!-- SECTION:FINAL_SUMMARY:END -->
