---
id: TASK-23
title: Stream PROGMEM MQTT payloads and remove 1200-byte payload scratch buffer
status: Done
assignee:
  - '@copilot'
created_date: '2026-03-19 17:04'
updated_date: '2026-03-19 18:04'
labels:
  - mqtt memory streaming
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the PROGMEM-to-RAM copy path in MQTT publishing with chunked streaming so flash-resident payloads are published without the permanent 1200-byte payload buffer. This is a high-payoff, relatively contained RAM optimization aligned with the firmware preference for chunked streaming over large buffers.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PROGMEM MQTT payload publishing no longer requires the 1200-byte persistent payload buffer
- [x] #2 MQTT publish paths preserve current behavior for retained and non-retained messages
- [x] #3 Implementation uses chunked streaming or a sub-64-byte staging buffer only where strictly required
- [x] #4 Debug and publish behavior remain unchanged for existing MQTT topics
- [x] #5 Build succeeds and MQTT publish regression is validated
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced the PROGMEM MQTT payload copy path with direct beginPublish streaming.

Changes:
- Removed the resident 1200-byte PROGMEM payload scratch buffer from sendMQTTData().
- Added a 63-byte staging buffer for flash-resident payload chunks only.
- Preserved retained and non-retained publish behavior and the existing MQTT debug output shape.

Validation:
- Full firmware build succeeded.
- Static publish-path sweep confirms outbound MQTT publishes now use beginPublish/write/endPublish rather than a copied RAM payload buffer.
<!-- SECTION:FINAL_SUMMARY:END -->
