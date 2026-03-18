---
id: TASK-21
title: Compact OT/MQTT publish tracking tables
status: To Do
assignee: []
created_date: '2026-03-18 19:44'
labels:
  - memory mqtt restapi core
dependencies: []
references:
  - 'src/OTGW-firmware/OTGW-Core.ino:221'
  - 'src/OTGW-firmware/OTGW-Core.ino:222'
  - 'src/OTGW-firmware/OTGW-Core.ino:223'
  - 'src/OTGW-firmware/OTGW-Core.ino:225'
  - 'src/OTGW-firmware/restAPI.ino:598'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OTGW-Core now keeps per-message tracking arrays for REST timestamps and MQTT throttling: msglastupdated[256], mqttlastsent[256], mqttlastsentstatusbit[16], plus first-seen bitmaps and status-byte timers. This block accounts for roughly 2.1KB of persistent RAM and is the largest single always-resident addition since v1.2.0. This task evaluates denser encodings and narrower scope, such as limiting storage to publishable/REST-exposed message IDs, reducing timestamp width where safe, or splitting normal/status paths into smaller dedicated tables. The aim is to reclaim 1KB or more without breaking MQTT interval gating or REST last-updated semantics.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Persistent RAM used by OT/MQTT tracking tables is reduced by at least 1024 bytes
- [ ] #2 MQTT publish throttling and first-seen behavior remain functionally equivalent
- [ ] #3 REST API last-updated timestamps remain available for the supported fields
- [ ] #4 No cross-slot contamination is introduced in MQTT gating logic
<!-- AC:END -->
