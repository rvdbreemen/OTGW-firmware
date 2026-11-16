---
id: TASK-2001
title: >-
  SAT: a simple weekly scheduler for the room setpoint (Sergeant D proposal,
  2.1.0)
status: To Do
assignee: []
created_date: '2026-11-16T12:50:00Z'
updated_date: '2026-11-16T14:29:00Z'
labels:
  - sat
  - feature
milestone: 2.1.0
dependencies: []
ordinal: 1023000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Proposed by Sergeant D for a release after 2.0.0, #dev-sat-mqtt https://discord.com/channels/812969634638725140/1105556725714649128/1571866919152128041 (2026-11-16 12:37 UTC): 'A simple weekly scheduler for room setpoint'. Design to be worked out with him (one question per message with the scope line); Robert decides scope and priority.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design agreed with Sergeant D and recorded as entries (time slots per weekday, interaction with presets and the HA thermostat card, behaviour across a reboot)
- [ ] #2 Robert's go recorded
- [ ] #3 Implementation with host tests red on the old code and green on the change, and a bench run
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Scope by Robert (2026-11-16 13:12 UTC, https://discord.com/channels/812969634638725140/1105556725714649128/1571875517676658750): 'That will be a 2.1.0 release or later.' Milestone 2.1.0 stays; needs-maintainer-ok stays until he gives the go.

2026-11-16 Robert released (DM 1571893376197074975, option A, relayed by the coordinator): planned for 2.1.0; design with Sergeant D after 2.0.0 stable. needs-maintainer-ok removed, milestone 2.1.0 kept.
<!-- SECTION:NOTES:END -->
