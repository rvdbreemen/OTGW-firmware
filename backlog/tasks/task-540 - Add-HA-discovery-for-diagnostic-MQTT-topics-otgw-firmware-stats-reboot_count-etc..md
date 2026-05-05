---
id: TASK-540
title: >-
  Add HA discovery for diagnostic MQTT topics (otgw-firmware/stats/*,
  reboot_count, etc.)
status: Done
assignee:
  - '@robert'
created_date: '2026-05-05 05:44'
updated_date: '2026-05-05 07:37'
labels:
  - mqtt
  - ha-discovery
  - diagnostic
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Audit during TASK-538 confirmed that every OT data ID has a HA discovery entry, but several runtime/diagnostic topics are still raw-published without discovery configs. Users wanting these as HA entities have to write manual configuration.yaml templates.\n\nIn-scope topic groups (currently raw-published):\n- otgw-firmware/reboot_count, reboot_reason, version, hostname\n- otgw-firmware/stats/* (disc_verify_runs, disc_republish_triggered, disc_last_missing, disc_published_topics, drip_burst_skip, drip_cooldown_skip — and any other counters added since)\n- otgw-pic/version, deviceid, firmwaretype, designer, picavailable (gate behind isPICEnabled())\n- otgw-pic/settings/* (reset_cause, led, led1..6, thermostat_detect)\n\nOut of scope here:\n- lockout_reset (button/trigger — separate task)\n- event_report (log stream)\n- Any per-OT-ID work (already covered)\n\nAll new entities should attach to the existing OTGW HA device, default to entity_category=diagnostic, and use sensible state_class / device_class where applicable (e.g. reboot_count → total_increasing). Reuse the existing MqttHaSensorCfg / streamSensorDiscovery pipeline rather than inventing a parallel one.\n\nReference points:\n- src/OTGW-firmware/mqtt_configuratie.cpp — discovery template arrays + streamSensorDiscovery\n- src/OTGW-firmware/MQTTstuff.ino — sendMQTTstats(), firmware/PIC publish helpers\n- ADR-065 (PIC subtree)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Discovery configs published for otgw-firmware/{reboot_count, reboot_reason, version, hostname}
- [x] #2 Discovery configs published for every otgw-firmware/stats/* topic
- [x] #3 Discovery configs published for otgw-pic/{version, deviceid, firmwaretype, designer, picavailable}, gated by isPICEnabled()
- [x] #4 Discovery configs published for otgw-pic/settings/{reset_cause, led, led1..6, thermostat_detect}
- [x] #5 All new entities attach to the existing OTGW HA device and default to entity_category=diagnostic
- [x] #6 Build (./build.sh) produces firmware + filesystem; evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify infrastructure: confirm pseudo-ID 247 (heap stats) discovery flow works generically (label = topic path under MQTTPubNamespace) and that MQTT_HA_FLAG_IS_PIC_ENTRY=0x08 gates publication when PIC disabled.
2. Define new pseudo-IDs in OTGW-firmware.h:
   - OTGWfwinfoid      = 248   (firmware info)
   - OTGWpicinfoid     = 249   (PIC info)
   - OTGWpicsettingsid = 250   (PIC settings, all 15 published topics)
3. Add ha_lbl_*/ha_name_* PROGMEM strings in mqtt_configuratie.cpp for:
   - 248: reboot_count, reboot_reason, version, hostname (4 entries)
   - 249: pic/version, deviceid, firmwaretype, designer, picavailable (5 entries, IS_PIC flag)
   - 250: setpoint_override, setback, dhw_override, gpio, gpio_states, led, tweaks, temp_sensor, smart_power, thermostat_detect, builddate, clock_mhz, reset_cause, standalone_interval, voltage_ref (15 entries, IS_PIC flag)
4. Append 24 new entries to mqttHaSensors[] (id 248/249/250), all HaEntityCat::diagnostic, retained=true. reboot_count uses total_increasing+counter; rest use none/information_outline.
5. Bump MQTT_HA_SENSOR_COUNT and update mqttHaSensorIndex[] if needed (verify whether pseudo-IDs require an index entry).
6. Add setMQTTConfigPending(OTGWfwinfoid)/picinfoid/picsettingsid to markAllMQTTConfigPending().
7. Build with ./build.sh; run evaluate.py --quick.
8. Verify produced discovery configs by inspecting MQTTHaDiscovery.cpp output paths or by reading streamSensorDiscovery to confirm topic shape.

Does NOT touch the publish path: led1..led6 split is out of scope per user decision (1a). Other pic/settings topics (15 total) ARE covered (2b).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Pre-impl scan findings:
- Stats discovery (AC #2) already DONE by TASK-346 — pseudo-ID 247 with 17 entries.
- pic settings published via publishAllPICsettings() in OTGW-Core.ino:823 — 15 topics, all gated by isPICEnabled().
- sLed is one composite 6-char string (LED A-F functions). led1..6 split out of scope (1a).
- Will cover all 15 pic/settings/* topics under one pseudo-ID (2b).

Implementation:
- 3 new pseudo-IDs (248/249/250) in OTGW-firmware.h.
- 24 PROGMEM label/name pairs + 24 mqttHaSensors[] entries in mqtt_configuratie.cpp.
- MQTT_HA_SENSOR_COUNT 306 -> 330; mqttHaSensorIndex[248..250] = 306/310/315.
- markAllMQTTConfigPending() now sets the three new pending bits.
- Stats discovery (AC #2) was already DONE by TASK-346 — verified, no extra work needed.
- led1..led6 not split per agreed scope (1a); composite "led" topic covered as one entity.
- All 15 published otgw-pic/settings/* topics covered (scope 2b).

Verification:
- ./build.sh produced firmware (0.70 MB) + filesystem (1.98 MB), 0 compile warnings.
- evaluate.py --quick: 91.7% health, matches baseline. HA Sensor Index Consistency check passes.

Committed as 6f4c9eff on dev. Not pushed yet — will push together with TASK-541 port to 2.0.0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Adds Home Assistant discovery configs for the runtime/diagnostic MQTT topics that were previously raw-published. Users can now expose reboot count/reason, firmware version+hostname, PIC info, and all 15 PIC settings topics as native HA entities without manual configuration.yaml templates.

Approach: reuses the existing MqttHaSensorCfg / streamSensorDiscovery pipeline (TASK-346 pattern) with three new pseudo-IDs:
- 248: otgw-firmware/{reboot_count,reboot_reason,version,hostname} — 4 entries
- 249: otgw-pic/{version,deviceid,firmwaretype,designer,picavailable} — 5 entries, IS_PIC flag auto-prepends prefix and gates publication on isPICEnabled()
- 250: otgw-pic/settings/* — all 15 published settings topics, IS_PIC flag

All 24 new entities are HaEntityCat::diagnostic and retained=true. led1..led6 split is out of scope (sLed is a 6-char composite "RFFTTT"; can be a follow-up if individual LEDs are needed).

Wiring: markAllMQTTConfigPending() sets the new pseudo-IDs as pending so the drip publisher handles them automatically. MQTT_HA_SENSOR_COUNT bumped 306 -> 330; mqttHaSensorIndex updated for IDs 248-250.

Files touched:
- src/OTGW-firmware/OTGW-firmware.h (3 new pseudo-ID constants)
- src/OTGW-firmware/mqtt_configuratie.cpp (24 PROGMEM strings + 24 array entries + count + index)
- src/OTGW-firmware/MQTTstuff.ino (3 setMQTTConfigPending calls)

Verification: ./build.sh produces firmware (0.70 MB) + filesystem (1.98 MB), 0 warnings. evaluate.py --quick 91.7% health (baseline match). HA Sensor Index Consistency evaluator check passes.

Follow-up: TASK-541 ports this to feature-dev-2.0.0-otgw32-esp32-sat-support and adds any 2.0.0-specific ESP32/OTDirect/SAT diagnostic topics.

Commit: 6f4c9eff. Local on dev, not pushed yet.
<!-- SECTION:FINAL_SUMMARY:END -->
