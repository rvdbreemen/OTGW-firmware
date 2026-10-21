---
id: TASK-1284
title: >-
  SAT MQTT state for integrations: publish state while disabled and after a
  safety trip, add retained enabled/preset/control_mode/fallback topics
status: To Do
assignee: []
created_date: '2026-10-21 18:11'
labels:
  - sat
  - mqtt
  - ha
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 413000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feedback gruijter 2026-10-21 (#alpha-testing, Homey app author), checked against dev d23ef153. Commands are fine; state is not reliable after a Homey or gateway reboot. 1) No state while SAT is disabled: satPublishMQTT() returns when !bEnabled (SATcontrol.ino ~2489) and satDisable() publishes nothing, so retained sat/active=true and sat/mode=continuous stay on the broker after sat/enabled false. 2) A safety trip is never published: both trip paths (~4515, ~4837) end in satDisable() without a publish, later loops return early (~4419); sat/safety_tripped is non-retained. Missing retained state topics: 3) sat/enabled (on/off matching the command; sat/active is not a substitute), 4) sat/preset and sat/control_mode (commands exist, no state to read back; sat/mode is the actual mode), 5) sat/fallback_active (REST only). 6) SATexternaltemp ('use MQTT/REST as indoor temp source') has no MQTT set or state topic. 7) HA discovery: the PV-boost number/switch entities use sat/boost_* while the firmware uses sat/pv_boost_* (MQTTstuff.ino ~2552-2554), so they are dead. Payload formats are fine. Mind ADR-111 (SAT publish on-change + heartbeat), ADR-101 flat topics, ADR-052 publish eligibility.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 After sat/enabled false the retained SAT state reflects disabled (sat/enabled false, sat/active false), checked with a broker capture
- [ ] #2 A safety trip publishes sat/safety_tripped (retained) and the disabled state
- [ ] #3 Retained state topics sat/enabled, sat/preset, sat/control_mode, sat/fallback_active exist and follow the commands
- [ ] #4 MQTT set + state for the external indoor-temp source (SATexternaltemp)
- [ ] #5 HA PV-boost entities use the topics the firmware publishes (discovery capture)
- [ ] #6 docs/api/MQTT.md and the manual list the new topics
<!-- AC:END -->
