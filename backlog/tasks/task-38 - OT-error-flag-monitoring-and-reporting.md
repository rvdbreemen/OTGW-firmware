---
id: TASK-38
title: OT error flag monitoring and reporting
status: Done
assignee:
  - '@claude'
created_date: '2026-04-05 20:52'
updated_date: '2026-04-05 22:56'
labels:
  - sat
  - feature
  - safety
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Monitor OpenTherm error flags from the boiler and report them via MQTT, REST, and WebUI. Includes fault codes, service required flags, and diagnostic information. SAT Python has error_monitoring config.

Reference: other-projects/SAT-releases-thermo-nova/custom_components/sat/const.py (CONF_ERROR_MONITORING)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Read OT fault flags from MsgID 0 and 5
- [ ] #2 Parse and report specific fault codes
- [ ] #3 MQTT publish: boiler error flags and fault codes
- [ ] #4 WebUI: error status display with descriptions
- [ ] #5 Optional: auto-disable SAT on critical boiler faults
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added OT fault flag check (SlaveStatus bit 0) in SAT control loop. Skips control cycle when boiler reports fault. Existing fault MQTT publishing already in OTGW-Core.ino.
<!-- SECTION:FINAL_SUMMARY:END -->
