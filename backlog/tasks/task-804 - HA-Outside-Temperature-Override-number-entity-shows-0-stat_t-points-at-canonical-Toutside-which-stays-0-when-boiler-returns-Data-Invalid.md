---
id: TASK-804
title: >-
  HA 'Outside Temperature Override' number entity shows 0: stat_t points at
  canonical /Toutside which stays 0 when boiler returns Data-Invalid
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-01 17:50'
updated_date: '2026-06-01 23:03'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - >-
    Discord #nederlandse-ondersteuning / Richard_HA / 2026-06-01 /
    Telnet_LOG_OTGW.txt
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported via Toutside investigation (Richard_HA, Discord #nederlandse-ondersteuning, 2026-06-01; OTGW fw 1.6.1, Remeha boiler without outside sensor).

The HA number entity 'Outside Temperature Override' is published by streamNumberDiscovery (src/OTGW-firmware/mqtt_configuratie.cpp:2659). Its command works (cmd_t = <subtopic>/outside -> firmware translates to OT=<val> and sends to PIC, confirmed in telnet capture: MQTT .../outside -> OT=20.5 -> sendOTGW). But its STATE topic is stat_t = <pubtopic>/Toutside (mqtt_configuratie.cpp:2701-2703), i.e. the canonical Toutside value.

When the boiler answers Data-Invalid on ID27 (common: boiler has no outside sensor), the ESP does not adopt the value (is_value_valid_for_master_topic, ADR-069/075) so canonical /Toutside stays 0. Result: the user sets the override (e.g. 20.5, gateway answers the thermostat with it), but the HA number control still displays 0. Confusing: the set works, the readback does not reflect it.

Possible directions (design choice, KISS - do not over-engineer):
1. Point stat_t at a topic that echoes the configured override (e.g. the thermostat-side source subtopic Toutside_thermostat when bSeparateSources is on, or a dedicated echo topic the firmware publishes when it accepts an OT= override).
2. Make the number entity optimistic / stateless (HA shows last-set value, no stat_t), accepting it won't reflect external changes.
3. Publish the accepted override value to a small dedicated state topic and point stat_t there.

Decide approach with maintainer before implementing. Code refs: mqtt_configuratie.cpp streamNumberDiscovery (~2659-2718), cmd handling /outside in MQTTstuff.ino handleMQTTcallback, canonical gate OTGW-Core.ino is_value_valid_for_master_topic / print_f88.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The HA 'Outside Temperature Override' number entity reflects the override value the user set, instead of showing 0 when the boiler returns Data-Invalid on ID27
- [x] #2 Approach chosen and recorded (stat_t retarget vs optimistic/stateless vs dedicated echo topic); maintainer sign-off on approach before implementation
- [x] #3 No regression for boilers that DO provide a valid Toutside (canonical sensor still shows the real value)
- [x] #4 python build.py exits 0 (firmware + filesystem)
- [x] #5 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01: Absorbed by TASK-805 (dev) / TASK-806 (2.0.0) — the override-state store retargets the Toutside_override number-entity stat_t. Track there; this task superseded by the broader all-overrides feature.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Superseded by TASK-805 (Done, commit 7d391106). The reported bug (HA 'Outside Temperature Override' number shows 0 when boiler answers Data-Invalid on ID27) is fixed there: TASK-805 added an override store and retargeted the Toutside_override number-entity stat_t from canonical /Toutside to /Toutside/override, so the entity now reflects the user-set override instead of the gated-to-0 canonical value. No separate code change required here. Approach (804 AC#2): dedicated override echo topic + stat_t retarget, chosen and shipped under ADR-082. Build/evaluate green in 805.
<!-- SECTION:FINAL_SUMMARY:END -->
