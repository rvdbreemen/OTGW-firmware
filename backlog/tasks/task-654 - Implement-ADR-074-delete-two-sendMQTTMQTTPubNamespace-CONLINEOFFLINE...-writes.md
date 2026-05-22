---
id: TASK-654
title: >-
  Implement ADR-074: delete two sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(...))
  writes
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-22 05:51'
updated_date: '2026-05-22 06:24'
labels:
  - adr-074
  - bug
  - mqtt
dependencies: []
references:
  - docs/adr/ADR-074-ha-availability-reflects-mqtt-link-not-ot-bus.md
  - src/OTGW-firmware/OTGW-Core.ino
  - src/OTGW-firmware/MQTTstuff.ino
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-074 (HA availability reflects MQTT link, not OT bus) was Accepted on 2026-05-16 but its mandated code change was never made on dev. Both writes still match the ADR-074 Enforcement-block forbid regex verbatim — the bug (DHW Control / Thermostat entities flap unavailable when OT bus is quiet) continues to ship to beta testers. ADR-judge did not block because bin/adr-judge is absent from this worktree.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 src/OTGW-firmware/OTGW-Core.ino:4044 sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline)) call removed
- [ ] #2 src/OTGW-firmware/MQTTstuff.ino:1158 sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline)) call removed
- [ ] #3 LWT/birth pair owns availability semantics exclusively; manual MQTT subscribe to <toptopic>/<hostname> confirms only "online"/"offline" from LWT/birth events
- [ ] #4 Field-validation in Discord #beta-testing confirms DHW Control / Thermostat entities no longer flap unavailable during OT-bus quiet periods
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 python evaluate.py --quick shows no new failures
- [ ] #7 Version prerelease bumped per CLAUDE.md versioning policy
<!-- AC:END -->
