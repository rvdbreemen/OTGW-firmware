---
id: TASK-1949
title: >-
  Review PR #702: Use L/min for the DHW flow rate unit in Home Assistant
  discovery
status: To Do
assignee: []
created_date: '2026-11-14T20:51:00Z'
labels:
  - github
  - pr
  - mqtt
  - homeassistant
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/pull/702 by justink0k (fork), base dev, opened 2026-11-14T20:41Z. One-line change in MQTTHaDiscovery.cpp:2138 haUnitStr(): HaUnit::l_min 'l/min' -> 'L/min'. Reported HA error: 'The unit of measurement l/min is not valid together with device class volume_flow_rate' for boiler_DHWFlowRate (MsgID 19), so the sensor is not created (firmware 2.0.0-alpha.416, HA 2026.11.1). Checked by comms: HA core const.py:653 LITERS_PER_MINUTE = 'L/min', sensor/const.py:711 VOLUME_FLOW_RATE allows set(UnitOfVolumeFlowRate) only. Other 'l/min' strings (not part of the PR): OTGW-Core.h:392 OT map unit, MQTTstuff.h:127/142 comments, docs/api/MQTT.md:241/744. The author has not built it. The claude-review run is 'action_required' (first-time contributor workflow approval).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reviewed against the HA unit list and every consumer of haUnitStr(HaUnit::l_min) (review comment on the PR)
- [ ] #2 Tests and the esp32-combo build green on the PR head
- [ ] #3 Bench: HA discovery creates the DHW flow rate sensor with unit L/min (HA log shows no unit error)
- [ ] #4 Robert decides on the merge; the author is credited in #alpha-testing
<!-- AC:END -->
