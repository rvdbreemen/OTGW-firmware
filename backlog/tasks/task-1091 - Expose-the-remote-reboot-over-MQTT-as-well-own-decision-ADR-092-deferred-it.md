---
id: TASK-1091
title: 'Expose the remote reboot over MQTT as well (own decision, ADR-092 deferred it)'
status: To Do
assignee: []
created_date: '2026-08-25 21:00'
labels:
  - enhancement
  - adr-required
dependencies: []
priority: low
ordinal: 190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-092 added a telnet reboot command as the recovery route for a device whose HTTP heap gate has engaged, and deliberately scoped MQTT out. This task carries that deferred decision.

The case for it: Home Assistant users are far more likely to have MQTT to hand than a telnet client, and a command surface already exists.

The case against, and why it needs its own ADR rather than a code change: an MQTT reboot is a new external effect on a channel shared with a broker, so it needs an explicit decision about who may send it. Telnet already sits inside ADR-032's trusted-local-network model, so adding a command there changed no trust boundary; MQTT does.

Correction worth carrying forward, established while grilling ADR-092: reachability is NOT the argument against MQTT. canPublishMQTT() gates publishing, but handleMQTT() runs in the same loop branch as telnet and before the HTTP gate, so an inbound command would most likely still arrive on a gated device.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A decision on who may send a reboot over MQTT is recorded in its own ADR before any code
- [ ] #2 If accepted: the MQTT path routes through the same deferred-reboot mechanism ADR-092 mandates, never ESP.restart() inline
<!-- AC:END -->
