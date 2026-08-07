---
id: TASK-1058
title: >-
  Fix: HA climate entity mode stays Onbekend/unknown after Home Assistant
  restart
status: To Do
assignee: []
created_date: '2026-08-07 20:14'
labels:
  - bug
  - mqtt
dependencies: []
priority: high
ordinal: 170000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by nico55 (Discord #nederlandse-ondersteuning, 2026-08-06/07) on 1.7.2+728426c. After every HA Core update HA restarts; the OTGW climate card then shows mode 'Onbekend' while current temp (Tr) and target (TrSet) render fine. An HA-side gateway reset does not recover it; only an ESP reboot does. Evidence from his own capture (transcript-20260807-211337): the capture contains a real HA restart at 21:20:22 ('Home Assistant went online!'), the availability topic is retained 'online', and all discovery configs are retained -- but topic central_heating/value/<id>/hvac_mode was published 0 times in 14 minutes and had no retained value in the broker's flush at subscribe. mode_stat_t of the climate config points at exactly that topic. publishHvacMode() (OTGW-Core.ino:1654) only publishes on change or on force, and the last-published value lives in RAM (mqttLastHvacMode), so a device that has been up for days with a stable mode never re-emits it. Per ADR-073 the homeassistant/status->online handler is deliberately a no-op (MQTTstuff.ino:663-667), so nothing re-sends state on HA restart. Same reasoning applies to hvac_action and any other change-gated, non-retained state topic.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Root cause confirmed with evidence: identify whether hvac_mode is published non-retained, never published since boot, or both
- [ ] #2 After an HA restart (homeassistant/status -> online) the climate entity shows a real mode within one publish cycle, without an ESP reboot
- [ ] #3 Fix does not reintroduce the ADR-073 bulk discovery republish storm; if the ADR-073 no-op decision must change, a superseding ADR is authored and Accepted first
- [ ] #4 hvac_action and other change-gated state topics are covered by the same fix or explicitly documented as out of scope
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 python evaluate.py --quick shows no new failures
- [ ] #7 Field validation: nico55 confirms entities survive an HA Core update on the fix build
<!-- AC:END -->
