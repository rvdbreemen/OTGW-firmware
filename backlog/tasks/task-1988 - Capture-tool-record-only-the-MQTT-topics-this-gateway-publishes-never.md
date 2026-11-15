---
id: TASK-1988
title: 'Capture tool: record only the MQTT topics this gateway publishes, never ''#'''
status: To Do
assignee: []
created_date: '2026-11-15T21:19:00Z'
updated_date: '2026-11-15T21:28:00Z'
labels:
  - capture
  - privacy
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1016000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15 (CLI via the coordinator, 22:1x and 22:2x: 'Follow your advice. A.'). gruijter's beta.250 capture held his whole zigbee2mqtt household because otgw_capture.py subscribes to '#' by default (otgw_capture.py --topic default '#'). The capture must by default record only what this gateway publishes itself: its own topic prefix (value/state/status/LWT of this device, node id from device/info) and its own Home Assistant discovery configs (homeassistant/+/<its node id>/...). Not other devices, not other OTGWs on the same broker, not command topics others publish (set/...). An explicit opt-in --mqtt-all-topics subscribes to '#' for someone who wants it on purpose, with a clear warning on the console and in summary.txt that other devices are in the capture.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Default: the MQTT subscription and the written mqtt.log hold only this gateway's own published topics and its own HA discovery configs; a fake broker stream with zigbee2mqtt/... and a second OTGW's topics: red on the current tool, green after
- [ ] #2 --mqtt-all-topics subscribes to '#' and prints and records a warning (console + summary.txt) that other devices are in the capture; test
- [ ] #3 The node id / prefix comes from the gateway (device/info, settings); without it the tool records nothing from MQTT rather than '#', and says so; README updated
<!-- AC:END -->
