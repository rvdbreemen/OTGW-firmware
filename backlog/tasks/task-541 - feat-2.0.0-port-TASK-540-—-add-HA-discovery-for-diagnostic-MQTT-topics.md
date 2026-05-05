---
id: TASK-541
title: 'feat-2.0.0: port TASK-540 — add HA discovery for diagnostic MQTT topics'
status: Done
assignee:
  - '@robert'
created_date: '2026-05-05 05:45'
updated_date: '2026-05-05 07:56'
labels:
  - mqtt
  - ha-discovery
  - diagnostic
  - port
  - feat-2.0.0
dependencies:
  - TASK-540
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port TASK-540 (HA discovery for otgw-firmware/* and otgw-pic/* diagnostic topics) to feature-dev-2.0.0-otgw32-esp32-sat-support.\n\nCheck whether the 2.0.0 branch publishes additional or differently-named diagnostic topics (ESP32, OTDirect, SAT). If so, include those in the discovery set. Also confirm the discovery framework on 2.0.0 still uses MqttHaSensorCfg or has been refactored — adapt accordingly.\n\nReference dev-branch implementation: TASK-540.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All topic groups from TASK-540 also covered on 2.0.0 (firmware/*, firmware/stats/*, pic/*, pic/settings/*)
- [x] #2 Any 2.0.0-specific diagnostic topics (ESP32 / OTDirect / SAT) added to the discovery set
- [x] #3 All new entities attach to the OTGW HA device and use entity_category=diagnostic
- [x] #4 Build 2.0.0 firmware + filesystem successfully
- [x] #5 evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate equivalent insertion points in 2.0.0 branch:
   - dev mqtt_configuratie.cpp -> 2.0.0 MQTTHaDiscovery.cpp (renamed)
   - dev OTGW-firmware.h pseudo-ID definitions (may differ if 2.0.0 reshuffled)
   - dev MQTTstuff.ino markAllMQTTConfigPending() (likely same pattern)
2. Replicate TASK-540 changes:
   - 3 new pseudo-IDs (248/249/250)
   - 24 PROGMEM ha_lbl_/ha_name_ pairs
   - 24 mqttHaSensors[] entries (id 248/249/250)
   - Bump MQTT_HA_SENSOR_COUNT, set mqttHaSensorIndex[248..250]
   - 3 setMQTTConfigPending() calls
3. Audit 2.0.0-specific diagnostic topics (AC #2):
   - ESP32 Core 3.x: any platform topics?
   - OTDirect mode: extra pic/* topics?
   - SAT (Smart Adaptive Thermostat) diagnostic publishes?
   Add discovery for any additional ESP32/OTDirect/SAT diagnostics found.
4. Build verify on 2.0.0 (ESP8266 only — ESP32-S3 blocked by env Python 3.9 vs 3.10+ req).
5. evaluate.py --quick.
6. Commit and ask before pushing (feature branch).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Port executed cleanly, mirroring dev commit 6f4c9eff:
- mqtt_configuratie.cpp on dev = MQTTHaDiscovery.cpp on 2.0.0 (file renamed in this branch).
- 248/249/250 entries identical to dev port; 251 added on top with 7 OTDirect/SAT diagnostic entries.
- TASK-543 created for the ~55 SAT user-facing topics (control/PID/cycle/4h-stats/safety/BLE measurements/pressure/weather + dynamic zone topics) with explicit design questions: primary vs diagnostic categorization, JSON template handling, dynamic zone discovery, ENABLE_SAT gating.

Build verification:
- Required Python 3.12 (brew install python@3.12) — espressif32 platform.py blocks Python 3.9 even for ESP8266 builds.
- PATH="/opt/homebrew/opt/python@3.12/libexec/bin:$PATH" ./build.sh produced full ESP8266 + ESP32-S3 artifacts cleanly.
- 0 new compile warnings introduced. Pre-existing warnings in PubSubClient/OneWire/OTGWSerial libs unchanged.
- evaluate.py --quick: 97.1% health (59 pass / 2 warn / 0 fail); HA Sensor Index Consistency check passes.

Committed as 0c932d6c on feature-dev-2.0.0-otgw32-esp32-sat-support; not pushed (feature branch policy requires explicit confirmation).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ports TASK-540 (HA discovery for diagnostic MQTT topics) from dev to feature-dev-2.0.0-otgw32-esp32-sat-support and adds 7 additional 2.0.0-specific diagnostic discovery entries. Users on the 2.0.0 branch now get the same firmware/PIC diagnostic HA entities as dev, plus OTDirect flame metrics and SAT BLE/pressure health sensors.

Approach: same MqttHaSensorCfg / streamSensorDiscovery pipeline as TASK-540 dev-side. Port lands in MQTTHaDiscovery.cpp (the renamed mqtt_configuratie.cpp on this branch).

Pseudo-IDs added:
- 248: otgw-firmware/{reboot_count,reboot_reason,version,hostname} — 4 entries
- 249: otgw-pic/{version,deviceid,firmwaretype,designer,picavailable} — 5 entries, IS_PIC flag
- 250: otgw-pic/settings/* — all 15 published settings topics, IS_PIC flag
- 251 (2.0.0-only): OTDirect flame metrics + SAT BLE health + SAT pressure status — 7 entries

All 31 new entities are HaEntityCat::diagnostic and retained=true. The wider SAT user-facing surface (~55 control/PID/cycle/4h-stats/weather/zone topics) is filed as TASK-543 because those are primary entities with non-trivial design decisions — not a fit for "diagnostic" categorization.

Wiring: markAllMQTTConfigPending() sets the four new pseudo-IDs as pending; mqttHaSensorIndex updated for 248..251; MQTT_HA_SENSOR_COUNT bumped 306 -> 337.

Files touched:
- src/OTGW-firmware/OTGW-firmware.h (4 new pseudo-ID constants)
- src/OTGW-firmware/MQTTHaDiscovery.cpp (31 PROGMEM strings + 31 array entries + count + indices)
- src/OTGW-firmware/MQTTstuff.ino (4 setMQTTConfigPending calls)

Verification: full ./build.sh succeeded for ESP8266 (0.70 MB firmware + 1.98 MB filesystem) AND ESP32-S3 (1.87 MB merged + 3.94 MB merged-full + dist zip), 0 new compile warnings. evaluate.py --quick 97.1% health, no regressions. Required Python 3.12 — installed via brew install python@3.12.

Follow-ups:
- TASK-543: SAT user-facing discovery (the 55+ primary entities).
- Dev environment now has Python 3.12 available — useful for future 2.0.0 builds.

Commit: 0c932d6c. Local on feature branch, not pushed yet — feature branch policy.
<!-- SECTION:FINAL_SUMMARY:END -->
