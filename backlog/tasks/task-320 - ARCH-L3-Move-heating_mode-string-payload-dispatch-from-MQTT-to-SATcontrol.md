---
id: TASK-320
title: '[ARCH-L3] Move heating_mode string-payload dispatch from MQTT to SATcontrol'
status: To Do
assignee: []
created_date: '2026-04-18 19:25'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.ino:607-614 has inner strcasecmp_P for eco/comfort payload, a two-level string-token discriminator. Belongs in SATcontrol.ino next to the heating-mode enum.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 satHandleHeatingMode(const char*) added in SATcontrol.ino
- [ ] #2 MQTTstuff callback reduced to a one-line delegation
- [ ] #3 Behavior unchanged for eco/comfort and numeric payloads
<!-- AC:END -->
