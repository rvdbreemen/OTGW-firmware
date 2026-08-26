---
id: TASK-1091
title: Document the Home Assistant recipe for cumulative DHW water consumption
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-26 19:23'
updated_date: '2026-08-26 19:57'
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
GH #675 follow-up, re-scoped. The reporter (Jeroenll) built the cumulative water total in Home Assistant itself, using the integration (Riemann sum) platform on the DHW flow rate sensor plus a template sensor carrying device_class water and state_class total_increasing, and confirmed it works on v1.7.4. That covers the Energy dashboard request without any firmware change. A device-side integrator was designed and deliberately rejected: MsgID 19 is not polled by the gateway, so its arrival cadence is install-dependent, and integrating it on the ESP8266 would carry a systematic error that HA's max_sub_interval handles correctly on the host side. Ship the recipe as documentation instead, with credit to the reporter and the reasoning for not building it into the firmware.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A guide in docs/guides/ describes the integration + template sensor recipe with working YAML, and names the entity ids a reader must substitute
- [x] #2 The guide states why the firmware does not ship its own cumulative counter (MsgID 19 arrival is thermostat-driven and install-dependent) and credits the reporter
- [x] #3 The guide notes the prerequisite that the DHW flow rate sensor must be present and valid, referencing the L/min unit fix (TASK-1092)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Re-scoped from a firmware feature to documentation, and shipped the recipe.

Why: the reporter on GH #675 had already built the cumulative water total in Home Assistant with the integration (Riemann sum) platform plus a template sensor, and confirmed it on v1.7.4. A device-side integrator was designed in full and then rejected: the gateway does not poll MsgID 19, so its arrival cadence is a property of each installation. Integrating that on the ESP8266 means multiplying a live flow reading by however long the bus happened to be quiet, producing an error no user can check. HA max_sub_interval solves exactly that on the host side, with a real clock and no cooperative scheduler to starve.

What changed: docs/guides/HA_DHW_WATER_METER.md, a new guide carrying the working YAML with credit to Jeroenll, a per-setting table explaining why method: left and max_sub_interval: 1min are there, a warning to substitute your own entity id, the prerequisite that the flow rate sensor must be valid (see TASK-1092 for the L/min fix that restores it after v1.7.5-beta.2), and a section recording why the firmware ships no counter of its own.

No firmware change, so no build gate applies. The four original ACs about an MQTT topic, a discovery entity, reboot persistence and metered accuracy were removed with the scope change.
<!-- SECTION:FINAL_SUMMARY:END -->
