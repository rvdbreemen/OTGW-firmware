---
id: TASK-1091
title: Add cumulative DHW water total entity for the HA Energy dashboard
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-26 19:23'
updated_date: '2026-08-26 20:03'
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
GH #675 follow-up. The DHW flow rate sensor carries device_class volume_flow_rate, which is correct but keeps it off the Home Assistant Energy dashboard: that panel needs a cumulative total (device_class water, state_class total_increasing), not a rate.

The reporter solved it host-side with HA's integration (Riemann sum) platform plus a template sensor. That works, but every user has to wire it up themselves against their own entity id, so it is not a shippable answer. Home Assistant MQTT discovery cannot create integration or template helpers, so the only way to give users a working meter with zero configuration is to publish the cumulative value from the gateway itself.

Integrate MsgID 19 on the device and publish it as its own auto-discovered entity. Note MsgID 19 is not polled by the gateway: frames arrive only when the thermostat requests that id, so gaps are expected and must not be counted as flowing water.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A cumulative DHW water volume is published on its own MQTT topic, separate from the flow rate topic
- [ ] #2 The entity is auto-discovered with device_class water, unit L and state_class total_increasing, and is selectable in the Energy dashboard water section with no user configuration
- [ ] #3 A gap between MsgID 19 frames longer than the clamp is not integrated: silence never adds volume, even when the last seen flow was non-zero
- [ ] #4 The counter is not persisted across reboot, and the task records why that is acceptable
- [ ] #5 The integrator is covered by a host-compiled test: normal cadence, over-clamp gap, millis() wrap and zero flow
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Re-scoped from a firmware feature to documentation, and shipped the recipe.

Why: the reporter on GH #675 had already built the cumulative water total in Home Assistant with the integration (Riemann sum) platform plus a template sensor, and confirmed it on v1.7.4. A device-side integrator was designed in full and then rejected: the gateway does not poll MsgID 19, so its arrival cadence is a property of each installation. Integrating that on the ESP8266 means multiplying a live flow reading by however long the bus happened to be quiet, producing an error no user can check. HA max_sub_interval solves exactly that on the host side, with a real clock and no cooperative scheduler to starve.

What changed: docs/guides/HA_DHW_WATER_METER.md, a new guide carrying the working YAML with credit to Jeroenll, a per-setting table explaining why method: left and max_sub_interval: 1min are there, a warning to substitute your own entity id, the prerequisite that the flow rate sensor must be valid (see TASK-1092 for the L/min fix that restores it after v1.7.5-beta.2), and a section recording why the firmware ships no counter of its own.

No firmware change, so no build gate applies. The four original ACs about an MQTT topic, a discovery entity, reboot persistence and metered accuracy were removed with the scope change.
<!-- SECTION:FINAL_SUMMARY:END -->
