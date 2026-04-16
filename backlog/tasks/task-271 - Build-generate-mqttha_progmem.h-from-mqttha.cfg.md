---
id: TASK-271
title: 'Build: generate mqttha_progmem.h from mqttha.cfg'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-15 19:07'
updated_date: '2026-04-15 20:16'
labels:
  - performance
  - mqtt
  - tooling
  - stap-1
dependencies: []
references:
  - src/OTGW-firmware/data/mqttha.cfg
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
mqttha.cfg (173KB, 345 data entries) is scanned fully on every doAutoConfigureMsgid() call via LittleFS. Convert it to a PROGMEM data structure so the file system layer is eliminated entirely from the discovery hot path.

This task produces the tooling and the generated artefact only. The actual firmware refactor is a separate task (depends on this one).

Deliverable: tools/generate_mqttha_progmem.py + src/OTGW-firmware/mqttha_progmem.h

Generated header contains:
- PROGMEM string arrays: one const char[] PROGMEM per unique topic template and message template
- PROGMEM entry table: struct MqttHaCfgEntry { uint8_t id; PGM_P topic; PGM_P msg; } — one entry per config line
- PROGMEM index: const uint16_t PROGMEM mqttHaCfgIndex[256] — maps OT message ID to first entry index in table, 0xFFFF if absent
- Const: MQTT_HA_CFG_COUNT (number of entries in table)

Multiple entries per ID are supported (ID=0 has ~30 entries for climate/binary_sensor/etc): the index points to the first, subsequent entries with the same ID follow contiguously in the table, terminated by a different ID or end of table.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 tools/generate_mqttha_progmem.py exists and is runnable with: python tools/generate_mqttha_progmem.py
- [x] #2 Script reads src/OTGW-firmware/data/mqttha.cfg, skips comment lines, parses id;topic;msg format
- [x] #3 Generated header src/OTGW-firmware/mqttha_progmem.h compiles without warnings as part of the firmware
- [x] #4 mqttHaCfgIndex[id] == 0xFFFF for all IDs not present in the config file
- [x] #5 mqttHaCfgIndex[id] points to the correct first entry for each ID present in the config
- [x] #6 All entries for the same ID are contiguous in mqttHaCfgTable[]
- [x] #7 Script is idempotent: running it twice produces identical output
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Generated tools/generate_mqttha_progmem.py and src/OTGW-firmware/mqttha_progmem.h from mqttha.cfg. Stats: 345 entries, 118 unique OT IDs, mqttHaCfgIndex[256] PROGMEM index, MQTT_HA_CFG_COUNT=345. All self-critique checks passed: no unescaped quotes, correct index values, proper static/constexpr qualifiers.
<!-- SECTION:FINAL_SUMMARY:END -->
