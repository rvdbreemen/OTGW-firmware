---
id: TASK-1091
title: Add cumulative DHW water total entity for the HA Energy dashboard
status: To Do
assignee: []
created_date: '2026-08-26 19:23'
updated_date: '2026-08-26 19:25'
labels:
  - bug
  - enhancement
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: medium
ordinal: 190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GH #675 follow-up. The DHW flow rate sensor now carries device_class volume_flow_rate as of 1.7.5-beta.2, which is correct but keeps it off the Home Assistant Energy dashboard: that panel needs a cumulative total (device_class water, state_class total_increasing, unit m3 or L), not a rate. Reporter Jeroenll asked for the Energy dashboard specifically, so the rate fix does not close their request. This task covers designing and shipping the separate cumulative entity: integrate the flow rate over time on the device, decide persistence across reboot, and publish it as its own HA discovery entity.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A cumulative DHW water volume value is published on its own MQTT topic, separate from the flow rate topic
- [ ] #2 The entity is auto-discovered in Home Assistant with device_class water and state_class total_increasing, and is selectable in the Energy dashboard water section
- [ ] #3 The counter survives a gateway reboot, or the chosen non-persistent behaviour is documented in the task with its rationale
- [ ] #4 Integration accuracy is validated against a known DHW draw and the observed error is recorded
<!-- AC:END -->
