---
id: TASK-22
title: Release MQTT autodiscovery workspace after publish sessions
status: To Do
assignee: []
created_date: '2026-03-18 19:45'
labels:
  - memory mqtt heap
dependencies: []
references:
  - 'src/OTGW-firmware/MQTTstuff.ino:42'
  - 'src/OTGW-firmware/MQTTstuff.ino:60'
  - 'src/OTGW-firmware/MQTTstuff.ino:1049'
  - 'src/OTGW-firmware/MQTTstuff.ino:1187'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff uses a lazy-allocated MQTTAutoConfigBuffers workspace containing line[1200], topic[200], msg[1200], and savedTopic[200], for about 2800 bytes plus allocator overhead. Compared to v1.2.0 this moved a static allocation to runtime heap, but once allocated it is kept forever. If low-heap measurements are taken after Home Assistant autodiscovery or reconnect republish, this retained block likely contributes materially to the observed drop. This task evaluates session-scoped allocation and release, or a smaller/reused workspace, while avoiding fragmentation or re-entrancy regressions.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Runtime heap is recovered after MQTT autodiscovery/reconnect publish work completes
- [ ] #2 MQTT autodiscovery remains functionally correct across reconnects
- [ ] #3 No use-after-free or re-entrancy bug is introduced in autoconfig code paths
- [ ] #4 Heap fragmentation risk is assessed and documented before merging
<!-- AC:END -->
