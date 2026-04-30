---
id: TASK-409
title: 'Remove 2.0.0 retained-cleanup migration code (target: 2.3.0 or 3.0.0)'
status: To Do
assignee: []
created_date: '2026-04-24 19:57'
updated_date: '2026-04-30 00:37'
labels:
  - migration-cleanup
  - future-work
  - mqtt
  - tech-debt
dependencies: []
priority: low
ordinal: 4000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up task for the temporary migration code added in 2.0.0. DO NOT START BEFORE the 2.3.0 milestone is opened, or unless a 3.0.0 release is being prepared.

Background:
In 2.0.0 the firmware gained a self-healing retained-cleanup block in src/OTGW-firmware/MQTTstuff.ino that subscribes briefly to six deprecated MQTT topic paths on every MQTT reconnect and wipes retained payloads it finds there. The deprecated paths are:
- OTGW/value/<uniqueId>/otgw-pic/boiler_connected
- OTGW/value/<uniqueId>/otgw-pic/thermostat_connected
- OTGW/value/<uniqueId>/otgw-pic/otgw_connected
- OTGW/value/<uniqueId>/otgw-otdirect/boiler_connected
- OTGW/value/<uniqueId>/otgw-otdirect/thermostat_connected
- OTGW/value/<uniqueId>/otgw-otdirect/ot_online

These were created by firmware <= 1.4.x. By the time a broker has run a 2.0.0 firmware at least once, these retained entries are gone. After enough field rotation, the cleanup block is dead weight.

Code to remove:
- kV2DeprecatedTopics[] array
- mqttV2MigrationOnConnect() helper
- mqttV2MigrationHandleIfDeprecated() helper
- mqttV2MigrationTick() helper
- TEMPORARY MIGRATION CODE header comment
- Call-sites in MQTT reconnect path, mqttCallback, and doBackgroundTasks
All are guarded by a single header comment block; follow that block's instructions.

BEFORE starting this task:
1. Check field-report channels (Discord #dev-sat-mqtt, GitHub issues, Tweakers OTGW topic) for the preceding 6 months for any mentions of stale retained state on otgw-pic/boiler_connected, otgw-pic/thermostat_connected, otgw-pic/otgw_connected, otgw-otdirect/boiler_connected, otgw-otdirect/thermostat_connected, or otgw-otdirect/ot_online.
2. If such reports exist, defer this task for at least another minor release.
3. Confirm ADR-084 is still the governing ADR; if superseded, follow the newer ADR's guidance.

Related: ADR-084 (generic OT-bus state topics), ADR-065 (otgw-pic MQTT subtree, amended by ADR-084). Both should be updated with a status note once this cleanup lands.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Task is NOT picked up before the 2.3.0 milestone is opened or 3.0.0 release work begins
- [ ] #2 Field-report check completed: no mentions of stale retained state on the six deprecated topics in the preceding 6 months, documented in task notes
- [ ] #3 src/OTGW-firmware/MQTTstuff.ino no longer contains kV2DeprecatedTopics[], mqttV2MigrationOnConnect, mqttV2MigrationHandleIfDeprecated, mqttV2MigrationTick, or the TEMPORARY MIGRATION CODE header comment
- [ ] #4 All call-sites in the MQTT reconnect path, mqttCallback, and doBackgroundTasks have been removed
- [ ] #5 python build.py --firmware clean for both ESP8266 and OTGW32 targets
- [ ] #6 python evaluate.py clean
- [ ] #7 Release notes for the target version contain an entry like 'Removed: 2.0.0 retained-cleanup migration helper (no user action required)'
- [ ] #8 ADR-084 status updated to reflect that migration is complete, or a new ADR supersedes ADR-084 documenting migration closure
<!-- AC:END -->
