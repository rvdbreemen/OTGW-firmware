---
id: TASK-242
title: 'Fix: OTGW flapping offline/online with serial overrun and MQTT throttle drops'
status: To Do
assignee: []
created_date: '2026-04-10 20:34'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing, user crashevans, 2026-04-10'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans in #beta-testing (2026-04-10). When many MQTT entities are enabled, the OTGW firmware appears to get into a processing backlog causing: repeated OTGW availability flaps (offline/online), MQTT throttle drops (40, 9, 7 msgs at a time), and at least one Serial Overrun event. The pattern suggests a throughput/buffering issue when MQTT output and debug logging get busy simultaneously. Reporter is on HA OS 2026.4.1, using built-in MQTT integration, OTGW beta firmware + PIC 6.6, with many OTGW MQTT entities enabled. Log attached to Discord message.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW connection remains stable (no offline/online flapping) under normal operation with many MQTT entities enabled
- [ ] #2 No serial overrun events in telnet log during normal operation
- [ ] #3 MQTT throttle drops reduced or eliminated under normal load
<!-- AC:END -->
