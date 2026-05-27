---
id: TASK-734
title: Write MQTT 5 client options report
status: In Progress
assignee:
  - '@codex'
created_date: '2026-05-27 21:08'
updated_date: '2026-05-27 21:10'
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
Document the impact of Home Assistant's MQTT protocol version 5 migration notice on OTGW firmware, with options for ESP8266 Arduino Core 2.7.4 and ESP32 MQTT client support. The output is a planning report under docs/plan for the HA 2027.1 MQTT5 support milestone.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Report is saved under docs/plan.
- [ ] #2 Report explains the Home Assistant MQTT 5 warning impact for this project.
- [ ] #3 Report compares Arduino Core 2.7.4 ESP8266 and ESP32 MQTT client/library options.
- [ ] #4 Report includes a concrete PubSubClient MQTT 5 extension plan, risks, and recommendation.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Inspect the existing docs/plan layout and local MQTT library baseline only as needed for accurate file references.
2. Add a markdown report under docs/plan summarizing HA MQTT5 impact, available ESP8266/ESP32 library options, and the PubSubClient MQTT5 fork path.
3. Validate the markdown diff and repository status, then update acceptance criteria and final summary in Backlog.
<!-- SECTION:PLAN:END -->
