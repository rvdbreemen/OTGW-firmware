---
id: TASK-131
title: 'SAT Audit F1: MQTT topics documentation'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:56'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-6
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Document all SAT MQTT topics published and subscribed by the firmware. Create an overview table with topic, direction, payload format, QoS and retain flag.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All published SAT topics documented
- [x] #2 All subscribed SAT topics documented
- [x] #3 Payload formats documented (JSON structure, data types)
- [x] #4 Document published in backlog/docs/sat-mqtt-topics.md
- [x] #5 Follow-up fix tasks created with label 'audit-fix' for missing documentation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Inventoried all published (80+) and subscribed (50+) SAT MQTT topics from SATcontrol.ino, SATble.ino, and MQTTstuff.ino. Existing MQTT.md SAT section only documented 8 published topics and 5 subscribed topics — massively incomplete. Created backlog/docs/sat-mqtt-topics.md with full table. Found one bug: mqttha.cfg references sat/current_modulation but no sendMQTTData call for that topic exists. Created task-209 (expand MQTT.md) and task-211 covers the current_modulation bug (task-213).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audited all SAT MQTT topics against firmware source and existing docs/api/MQTT.md. Found the SAT section in MQTT.md documents only 8 of 80+ published topics and 5 of 50+ subscribed topics. Created backlog/docs/sat-mqtt-topics.md as a complete reference covering all published topics (with retain flags), all subscribed command topics, JSON payload formats, and value tables for boiler status codes, cycle classifications, and flame status codes. Found one real bug: mqttha.cfg defines HA entity sat_current_modulation pointing at topic sat/current_modulation, but that topic is never published by the firmware (task-213 created). Created audit-fix task-209 to update MQTT.md from the new doc.
<!-- SECTION:FINAL_SUMMARY:END -->
