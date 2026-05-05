---
id: TASK-540
title: >-
  Add HA discovery for diagnostic MQTT topics (otgw-firmware/stats/*,
  reboot_count, etc.)
status: To Do
assignee: []
created_date: '2026-05-05 05:44'
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
- [ ] #1 Discovery configs published for otgw-firmware/{reboot_count, reboot_reason, version, hostname}
- [ ] #2 Discovery configs published for every otgw-firmware/stats/* topic
- [ ] #3 Discovery configs published for otgw-pic/{version, deviceid, firmwaretype, designer, picavailable}, gated by isPICEnabled()
- [ ] #4 Discovery configs published for otgw-pic/settings/{reset_cause, led, led1..6, thermostat_detect}
- [ ] #5 All new entities attach to the existing OTGW HA device and default to entity_category=diagnostic
- [ ] #6 Build (./build.sh) produces firmware + filesystem; evaluate.py --quick shows no new failures
<!-- AC:END -->
