---
id: TASK-2986
title: Bench MQTT broker on the NAS instead of the laptop
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T00:21:00Z'
updated_date: '2026-11-21T00:36:00Z'
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
- [x] #2 bench.sh mqtt job subscribes to the NAS broker; one bench unit publishes there and mqtt.log shows its topics
- [x] #3 docs/guides/agent-build-and-bench.md names the NAS broker and the unit settings change
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 coordinator (Robert via BlueFlame: no broker on the laptop; HA or NAS, coordinator chose NAS to keep bench retained/discovery topics out of production HA). 01:2x: container otgw-bench-mqtt on terradisk, listener 192.168.88.36:1883 (existing mosquitto-mqtt container keeps 127.0.0.1:1883), anonymous, persistence false, restart unless-stopped, /etc/localtime mounted. Pub/sub test NAS->NAS and laptop->NAS ok. 01:27:25 .61 mqttbroker 192.168.88.32 -> 192.168.88.36, 01:27:26 .63 homeassistant.local -> 192.168.88.36; settings.ini diff = only MQTTbroker on both; both mqttconnected true, ESTABLISHED on NAS:1883, broker log shows OTGW1020BA21B4F8 (.61) and OTGWAC276ECE45D8 (.63), OTGW/value topics flow. Recreated the container at 01:29:50 for local time; both reconnected within 20 s. bench.sh mqtt job -> 192.168.88.36 (sha 7b8e73cc, NAS = repo); test run brokertest-61: 58 lines in 40 s. Laptop broker: 0 clients after the switch (.68 uses homeassistant.local). Laptop broker NOT stopped: it runs under the scheduled task 'OTGW LAN mosquitto' (supervisor restarts it); disabling that task is a persistent config change on Robert's PC, asked in the session. Evidence: OTGW-field-data/bench-broker/2026-11-21_nas-broker/.

AC1 open: restart policy unless-stopped is set, but a NAS reboot was not tested (no reboot of Robert's NAS without his OK).

01:4x: otgw-dev (Robert's direct instruction: use the HA broker, credentials from settings.ini) changed the NAS bench.sh: the mqtt job now reads the broker from the unit's /settings.ini through new bin/mqttcfg.py (mDNS for .local, password via a temporary mode-600 .mqtt.env deleted after docker run). Synced into scripts/nas-bench/ (bench.sh c81ed355, mqttcfg.py 5ff2c9e2, equal to the NAS). Guide updated. Which broker the units use (NAS .36 vs HA) is open with Robert via the coordinator; units stay on .36 until then.

Robert chose A (DM 1573497099763585025, via coordinator): bench units stay on the NAS broker 192.168.88.36; for a real HA test point one unit at homeassistant.local temporarily. Guide updated. Laptop broker (scheduled task 'OTGW LAN mosquitto'): asked Robert in the session, waiting for his yes before disabling it.
<!-- SECTION:NOTES:END -->
