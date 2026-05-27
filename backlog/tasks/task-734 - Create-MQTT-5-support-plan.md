---
id: TASK-734
title: Create MQTT 5 support plan
status: Done
assignee:
  - '@codex'
created_date: '2026-05-27 21:08'
updated_date: '2026-05-27 22:06'
labels: []
milestone: m-0
dependencies: []
references:
  - libraries/PubSubClient/src/PubSubClient.h
  - libraries/PubSubClient/src/PubSubClient.cpp
documentation:
  - docs/plan
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Create a planning artifact for Home Assistant 2027.1 MQTT protocol version 5 support in OTGW firmware. The output must stay in docs/plan and must not implement firmware or library changes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plan is saved under docs/plan.
- [x] #2 Plan explains the Home Assistant MQTT 5 warning impact for this project.
- [x] #3 Plan compares viable ESP8266 Arduino Core 2.7.4 and ESP32 MQTT support paths only as planning input.
- [x] #4 Plan includes recommended phases, validation gates, and the PubSubClient MQTT 5 fork/spike scope.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Convert the docs/plan artifact into a plan-focused markdown file.
2. Keep scope to planning only: HA impact, support options, recommended phases, validation gates, and PubSubClient fork/spike boundaries.
3. Validate the markdown diff and repository status, then update acceptance criteria and final summary in Backlog.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Created docs/plan/HA_2027_01_MQTT5_SUPPORT_PLAN.md as a plan-only artifact. No firmware or MQTT library files were changed. Validation: checked the markdown file for trailing whitespace; repository status reviewed after the change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Created a plan-only MQTT5 support artifact at docs/plan/HA_2027_01_MQTT5_SUPPORT_PLAN.md. The plan records the current HA warning interpretation, recommends no immediate firmware change for standard Mosquitto setups, compares ESP8266 Core 2.7.4 and ESP32 MQTT support paths, and defines phased gates for any future PubSubClient MQTT5 spike. No firmware or library implementation was changed. Validation: checked the new markdown file for trailing whitespace and reviewed repository status.
<!-- SECTION:FINAL_SUMMARY:END -->
