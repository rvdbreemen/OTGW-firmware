---
id: TASK-307
title: >-
  [ARCH-M3] Replace 76-branch SAT MQTT strcasecmp chain with dispatch table
  (plus ADR)
status: To Do
assignee: []
created_date: '2026-04-18 19:21'
labels:
  - architecture
  - review-2026-04-18
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.ino:477-620 uses chained strcasecmp_P for 76 SAT subcommand tokens. Violates CLAUDE.md typed-control-flow rule. TASK-292 was a silent miss from this exact pattern. kV2Routes[] shows precedent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 New ADR describes MQTT subcommand dispatch table pattern
- [ ] #2 kSatMqttCmds[] PROGMEM table replaces the chained strcasecmp block
- [ ] #3 All 76 existing tokens land in the table; regression test confirms each routes correctly
<!-- AC:END -->
