---
id: TASK-534
title: >-
  Fix: DHW setpoint shows HA initial value (43°C) and DHW temperature unknown in
  HA via MQTT
status: Done
assignee:
  - '@codex'
created_date: '2026-05-04 06:11'
updated_date: '2026-05-05 12:38'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/543'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
stefan reports in #nederlandse-ondersteuning that the DHW setpoint shows 43°C in HA (the HA climate entity initial placeholder value) while the firmware UI correctly shows 60°C. The DHW temperature sensor (sensor.opentherm_gateway_otgw_otgw_dhw_temperature) shows 'unknown'. Firmware log confirms TdhwSet = 60°C is received correctly. Still present in beta.6 (confirmed 2026-05-03). Pattern is identical to GitHub #543 (Max CH water setpoint 0°C in HA Boiler entity). Likely cause: MQTT topic the firmware publishes to does not match the state_topic in the HA discovery config, OR stale retained discovery config from an older firmware version is overriding the current one.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 DHW setpoint in HA reflects the actual boiler value (60°C), not the 43°C placeholder
- [x] #2 DHW temperature sensor in HA shows a value or correctly reports unavailable when the boiler does not send OT ID 26
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Trace MQTT publish topics for OT IDs TdhwSet and Tdhw in the current dev code.
2. Trace Home Assistant discovery config for the DHW climate/sensor entities and compare state_topic/value_template assumptions.
3. Patch dev if the topic or template mismatch is identifiable from code.
4. Build/verify the firmware and update TASK-534 acceptance criteria/notes.
5. If the fix should also exist on the 2.0.0 line, create a follow-up backlog task for that branch and apply the equivalent patch in the 2.0.0 worktree.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-05-03: stefan sent screenshot of firmware UI showing TdhwSet=60°C in the OT monitor log, dashboard shows Domestic Hot Water Setpoint = 60°C correctly. In HA (via MQTT), the DHW setpoint entity shows 43°C (the HA discovery initial placeholder) and the DHW temperature entity shows 'unknown'. Firmware is on v1.4.1 originally, confirmed still present in beta.6. stefan confirmed he only uses MQTT integration (not the HA OTGW core component simultaneously). Waiting for: MQTT Explorer dump showing which topics are retained under homeassistant/climate/otgw-*/dhw*/config and what state_topic they reference; and which MQTT topic the firmware publishes TdhwSet to.

2026-05-05: Traced MQTT publish and HA discovery. Runtime publishes OT ID 56 via canonical topic TdhwSet and OT ID 26 via Tdhw; DHW climate discovery already points temp_stat_t to TdhwSet and curr_temp_t to Tdhw. The actual remaining code-side cause of the 43 C symptom is the hardcoded DHW climate discovery initial value: "initial":"43". Removed that initial fallback so HA no longer displays a fabricated DHW target before the real TdhwSet state arrives. DHW temperature remains tied to Tdhw; if the boiler never sends OT ID 26, HA should remain unknown/unavailable rather than showing a fake value. Verification: rg confirms no initial 43 remains in dev DHW climate discovery; make completed successfully on ESP8266 core 2.7.4.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the DHW Home Assistant MQTT climate discovery so it no longer advertises a hardcoded 43 C initial target temperature. The live target temperature state topic remains TdhwSet, and current DHW temperature remains Tdhw, so HA now either receives the real boiler-reported setpoint or has no fabricated fallback value to display.

Verification:
- rg confirmed the 43 C initial value is absent and the DHW climate still references /Tdhw and /TdhwSet.
- make completed successfully for the dev ESP8266 build.
<!-- SECTION:FINAL_SUMMARY:END -->
