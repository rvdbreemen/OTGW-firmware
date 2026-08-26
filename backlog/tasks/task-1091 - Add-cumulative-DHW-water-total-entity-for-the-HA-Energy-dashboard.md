---
id: TASK-1091
title: Document the Home Assistant recipe for cumulative DHW water consumption
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-26 19:23'
updated_date: '2026-08-26 19:51'
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
- [ ] #1 A guide in docs/guides/ describes the integration + template sensor recipe with working YAML, and names the entity ids a reader must substitute
- [ ] #2 The guide states why the firmware does not ship its own cumulative counter (MsgID 19 arrival is thermostat-driven and install-dependent) and credits the reporter
- [ ] #3 The guide notes the prerequisite that the DHW flow rate sensor must be present and valid, referencing the L/min unit fix (TASK-1092)
<!-- AC:END -->
