---
id: TASK-2986
title: Move the bench MQTT broker (and the mqtt job) from the laptop to the NAS
status: To Do
assignee:
  - '@claude'
created_date: '2026-11-21T00:21:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1115000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The NAS bench.sh mqtt job subscribes to the mosquitto broker on the laptop (192.168.88.32), and the bench units publish there. Robert 2026-11-21: all measurements run on the NAS. Run a mosquitto broker as a container on terradisk, point the bench units and bench.sh at it, keep the laptop broker until every unit has moved. Coordinator: low priority, after the 08:00 beta soak and the 2949.5 reruns.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A mosquitto container runs on the NAS and survives a NAS reboot
- [ ] #2 bench.sh mqtt job subscribes to the NAS broker; one bench unit publishes there and mqtt.log shows its topics
- [ ] #3 docs/guides/agent-build-and-bench.md names the NAS broker and the unit settings change
<!-- AC:END -->
