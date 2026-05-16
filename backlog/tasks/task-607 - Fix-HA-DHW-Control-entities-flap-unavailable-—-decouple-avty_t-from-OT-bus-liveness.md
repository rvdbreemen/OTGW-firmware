---
id: TASK-607
title: >-
  Fix: HA DHW Control & entities flap unavailable — decouple avty_t from OT-bus
  liveness
status: To Do
assignee: []
created_date: '2026-05-16 07:18'
labels:
  - bug
  - mqtt
  - ha-discovery
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two field testers on 1.5.0 report the Home Assistant 'DHW Control' (and Thermostat) entity flapping available/unavailable (one measured ~20s off / 2s on; another 'every 2s'), no DHW settings shown, and DHW temperature/on-off controls dead. Root cause: every HA discovery entity sets avty_t to the base namespace topic <toptopic>/value/<nodeid>, and that same topic is overwritten with online/offline derived from OT-bus liveness (bBoilerState||bThermostatState, a 30s wall-clock window on time(nullptr)). Bursty OT traffic or late/unstable NTP flaps the base topic, so ALL HA entities flap; DHW Control is most visible because it is an interactive climate card. Regressed at 1.5.0 when /gateway was removed and online/offline moved onto the canonical base topic (TASK-538). Fix: stop writing OT-bus liveness to the base/avty_t topic; HA availability must reflect only the ESP<->MQTT link (birth online + LWT offline). OT-bus liveness stays on the dedicated otgw_connected sensor. 30s liveness window is left intact (it correctly drives otgw_connected).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Base namespace topic no longer receives OT-bus online/offline: both sendMQTT(MQTTPubNamespace, CONLINEOFFLINE(state.otgw.bOnline)) call sites removed (OTGW-Core.ino ~4029 and MQTTstuff.ino ~1113)
- [ ] #2 otgw_connected sensor still publishes OT-bus liveness unchanged (sendMQTTDataPic otgw_connected companion lines retained)
- [ ] #3 MQTT birth (online retained on connect) and LWT (offline retained) on the base topic remain intact and unchanged
- [ ] #4 HA DHW Control, Thermostat, and sensor/binary-sensor entities remain available while MQTT is connected, independent of OT-bus traffic gaps
- [ ] #5 New ADR (docs/adr/ADR-074) authored with >=2 alternatives, consequences, Enforcement block forbidding reintroduction of sendMQTT(MQTTPubNamespace, CONLINEOFFLINE; Status moved to Accepted only after explicit user approval
- [ ] #6 python build.py --firmware exits 0
- [ ] #7 python evaluate.py --quick shows no new failures
- [ ] #8 _VERSION_PRERELEASE bumped via bin/bump-prerelease.sh and version.h + data/version.hash staged with the firmware change
<!-- AC:END -->
