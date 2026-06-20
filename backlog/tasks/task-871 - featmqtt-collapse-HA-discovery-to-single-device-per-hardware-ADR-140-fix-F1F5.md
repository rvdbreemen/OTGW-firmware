---
id: TASK-871
title: >-
  feat(mqtt): collapse HA discovery to single device per hardware (ADR-140), fix
  F1+F5
status: In Review
assignee: []
created_date: '2026-06-15 14:21'
updated_date: '2026-06-20 11:31'
labels: []
dependencies: []
ordinal: 87000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-140 (supersedes ADR-124): revert the seven-device HA topology to ONE device per hardware OTGW (mental model: 1 hardware = 1 IP = 1 MQTT device). Hard-remove the seven-device machinery in MQTTHaDiscovery.cpp + the .ino twin: HaDevice 7-enum (MQTTstuff.h:376), deviceForOTId map (MQTTstuff.ino:2733), the bilateral Boiler/Thermostat two-pass (MQTTstuff.ino:2785-2853), via_device hub (MQTTHaDiscovery.cpp:2365-2371, MQTTstuff.ino:2413-2418), per-device suffix/metadata, and the g_haDeviceIntroduced[] array (MQTTstuff.ino:139). Emit ONE device block (shared identifiers=nodeId), full block (mfr/model/name='OpenTherm Gateway (<hostname>)'/sw_version) only on the first entity via a DRIVER-SET isFirstEntity gate (the legacy-style gate set by the caller, not mutated inside compose). This removes review finding F1 (MQTTHaDiscovery.cpp:2373 MEASURE-pass mutation of deviceIntroduced) by construction. Fold in F5 in the SAME change: JSON-escape hostname, manufacturer and model when written into the device block (once the single device block reliably emits, an unescaped quote/backslash would break the device-metadata JSON that F1 currently masks). Match the 1.6.x reference (OTGW-firmware/src/OTGW-firmware/mqtt_configuratie.cpp:1926-1948). Retire the bLegacyMode topology axis (never persisted/parsed); leave bUseLegacyOtTopics (ADR-106 topic naming) untouched. Emit a retained-discovery clear on the old per-device topics where feasible to reduce HA orphans.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ADR-140 is Accepted before this lands
- [ ] #2 HaDevice enum, deviceForOTId, bilateral two-pass, via_device, per-device metadata, and deviceIntroduced[] are removed
- [ ] #3 Discovery emits exactly one HA device (shared identifiers=nodeId); full device block on first entity only, bare ids on the rest; no via_device
- [ ] #4 F1 gone: first-entity gate is driver-set; MEASURE and WRITE passes produce identical length (no compose-time mutation)
- [ ] #5 F5 gone: hostname/manufacturer/model are JSON-escaped in the device block
- [ ] #6 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
- [ ] #7 Field-validation: captured discovery dump on a real device shows one device with all entities bound, matching the 1.6.x layout
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
DESIGN REVISION (ADR-140 refined 2026-06-15, maintainer): the 7 former devices become 7 CATEGORIES within ONE device, not flattened. So AC2 is corrected: the HaDevice enum, deviceForOTId map, and the bilateral Boiler/Thermostat handling are RETAINED and REPURPOSED to drive a category name-prefix (e.g. 'Boiler Control Setpoint', 'Thermostat Room Setpoint', 'ESP Free Heap'). ONLY the multi-DEVICE emission is removed: the 7 separate dev blocks, via_device, per-device suffix/metadata, and the deviceIntroduced[] per-device array. Mental model: 1 hardware = 1 IP = 1 MQTT device, with the 7 groupings as in-device categories. The actual category-prefix wiring is TASK-872; this task (871) is the single-device collapse + F1 (driver-set isFirstEntity) + F5 (escaping). HA constraint: only 3 native sections exist, so the 7 categories render as a name-prefix (entity_category stays a secondary axis for config/diagnostic).

LIVE HA discovery validated on test-rig broker 192.168.1.234 (device provisioned + MQTT connected, 2026-06-20): our device otgw-AC276ECE45D8 published 105 entities (disc_published_topics=107, disc_pending=0, disc_republish_triggered=0), ALL bound to ONE HA device identifier, via_device=0 across every published entity; uniq_id source prefixes present (otd_/pic_/esp_/sat_). Captured via mosquitto_sub homeassistant/# retained. AC7 (single device, all entities bound, no leaked per-device identifiers) is now LIVE-VALIDATED. The BLE via_device carve-out (AC3 divergence) did NOT manifest in the dump because no BLE probes are present; maintainer decided 2026-06-20 to AMEND ADR-140 to sanction BLE child-devices (via_device), so the divergence becomes compliant once the amending ADR is Accepted (to be authored via adr-kit). Remaining: that ADR amendment, then AC3 reconciles. Build: 3-target green at alpha.224+cdc4ec7.

AC#3 / BLE via_device divergence RESOLVED-BY-DECISION: maintainer chose (2026-06-20) to AMEND ADR-140 rather than remove the BLE child-device via_device. Drafted Proposed ADR-148 (docs/adr/ADR-148-ble-sensors-as-ha-child-devices-via-device.md, guideline-level) sanctioning BLE probe sensors as HA child-devices (via_device -> main OTGW device), all non-BLE entities stay in the single device per ADR-140. adr-generator verified via_device==uniqueId==nodeId chain. REMAINING before Done: (1) maintainer accepts ADR-148 via adr-kit gates; (2) reconcile AC#2/AC#3 checkbox text against the amended contract; (3) AC#6 esp32/esp32-combo build receipt (3-target green at alpha.224+cdc4ec7). AC7 single-device LIVE-validated 2026-06-20. Stays In Review.
<!-- SECTION:NOTES:END -->
