---
id: TASK-1348
title: 'MQTT: publish otgw-firmware/version (and the PIC version info) retained'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T10:41:00Z'
updated_date: '2026-10-24T14:06:00Z'
labels:
  - mqtt
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 471000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter 2026-10-24 10:23 UTC (#alpha-testing msg 1563498170913128469, Homey app): make the firmware version retained. Today sendMQTTversioninfo() (MQTTstuff.ino:1667) publishes otgw-firmware/version, reboot_count, reboot_reason and the pic/* version info without retain (hostname, hardware_type, hardware_mode already retained), so a client that subscribes after the boot burst does not see the version. Version info is static per boot: retain version and the pic version/deviceid/firmwaretype/designer; keep reboot_count/reason as they are unless reviewed. Update docs/api/MQTT.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otgw-firmware/version and pic/version arrive retained (mosquitto_sub -v -R after boot shows them; capture before/after on the bench)
- [x] #2 docs/api/MQTT.md marks them retained
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24 11:23 UTC scope narrowed by Robert (#alpha-testing 1563513134977650830): retain ONLY otgw-firmware/version (Homey needs it at pairing, gruijter 1563512791191785535). Not the PIC info and not other values: retained values give a false impression after an OTGW reboot.

2026-10-24 evidence (bench OTGW32, temp broker 192.168.88.32, mosquitto_sub --retained-only 'OTGW/value/+/otgw-firmware/#'): alpha.491 (old) retained = hostname, hardware_type, hardware_mode, NO version; after OTA of the fix: retained includes 'otgw-firmware/version 2.0.0-alpha.491+0d27967'. Scope per Robert: version only, PIC info not retained. MQTT.md row updated. build SUCCESS, evaluate --quick 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
otgw-firmware/version is published retained, so an integration that subscribes later (Homey at pairing, gruijter) gets it at once. Only that topic, per Robert 2026-10-24 (no other values retained: a retained value suggests the OTGW is online). Evidence: bench broker shows no retained version on alpha.491 and the retained version after the fix. MQTT.md updated.
<!-- SECTION:FINAL_SUMMARY:END -->
