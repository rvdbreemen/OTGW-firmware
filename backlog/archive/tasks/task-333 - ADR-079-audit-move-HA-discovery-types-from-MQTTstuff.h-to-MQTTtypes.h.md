---
id: TASK-333
title: 'ADR-079 audit: move HA-discovery types from MQTTstuff.h to MQTTtypes.h'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-19 18:34'
updated_date: '2026-04-19 19:31'
labels:
  - architecture
  - adr-079
  - cleanup
  - review-2026-04-18-followup
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTTstuff.h has grown to 361 lines and contains 9 MQTT type declarations that belong in MQTTtypes.h per ADR-079 (types live in <Component>types.h, not in the functional stuff.h). Types to move: HaDeviceClass, HaUnit, HaStateClass, HaIcon, HaEntityCat enums + MqttHaSensorCfg, MqttHaBinSensorCfg, HaDiscoveryContext, MqttJsonWriter structs/classes. After the move MQTTstuff.h becomes pure interface (macros + function decls + PubSubClient helpers) and MQTTtypes.h is the single canonical location for every MQTT type.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All 5 Ha* enums and all 4 MqttHa*/HaDiscoveryContext/MqttJsonWriter structs live in MQTTtypes.h
- [x] #2 MQTTstuff.h contains no struct/class/enum definitions — only macros, function decls, and helpers
- [x] #3 Both ESP8266 and ESP32-S3 build clean; MQTTHaDiscovery.cpp still compiles (it already includes MQTTstuff.h)
- [x] #4 evaluate.py check_adr_gates remains clean; no regression in tests/test_evaluate.py
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Direction reversed per ADR-081: instead of moving types OUT of MQTTstuff.h, we merged MQTTtypes.h IN. Reason: MQTT has a stuff.h sibling, so ADR-081 puts all types there. Net file-count reduction of 1 (MQTTtypes.h deleted). Self-contained HOME_ASSISTANT_DISCOVERY_PREFIX guard added so MQTTHaDiscovery.cpp (standalone TU that includes MQTTstuff.h but not OTGW-firmware.h) still compiles.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
MQTTtypes.h merged into MQTTstuff.h per ADR-081. MQTTRuntimeSection and MQTTSettingsSection now live in a labeled 'State + Settings sections' block near the top of MQTTstuff.h, before the existing PROGMEM helpers and HA-discovery enums. MQTTstuff.h adds an idempotent #ifndef/#define for HOME_ASSISTANT_DISCOVERY_PREFIX so standalone TUs (MQTTHaDiscovery.cpp) still build without pulling in OTGW-firmware.h. OTGW-firmware.h now includes MQTTstuff.h early (replacing the old MQTTtypes.h include slot) so OTGWState/OTGWSettings can reference the sections; later .ino includes become no-ops via #pragma once. MQTTtypes.h deleted. ESP32-S3 + ESP8266 both SUCCESS with real pio builds (Python 3.12 + PIO 6.1.19).
<!-- SECTION:FINAL_SUMMARY:END -->
