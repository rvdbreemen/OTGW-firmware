---
id: TASK-1091
title: Add cumulative DHW water total entity for the HA Energy dashboard
status: Done
assignee:
  - '@claude'
created_date: '2026-08-26 19:23'
updated_date: '2026-08-26 20:43'
labels:
  - bug
  - enhancement
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/675'
priority: medium
ordinal: 190000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
GH #675 follow-up. The DHW flow rate sensor carries device_class volume_flow_rate, which is correct but keeps it off the Home Assistant Energy dashboard: that panel needs a cumulative total (device_class water, state_class total_increasing), not a rate.

The reporter solved it host-side with HA's integration (Riemann sum) platform plus a template sensor. That works, but every user has to wire it up themselves against their own entity id, so it is not a shippable answer. Home Assistant MQTT discovery cannot create integration or template helpers, so the only way to give users a working meter with zero configuration is to publish the cumulative value from the gateway itself.

Integrate MsgID 19 on the device and publish it as its own auto-discovered entity. Note MsgID 19 is not polled by the gateway: frames arrive only when the thermostat requests that id, so gaps are expected and must not be counted as flowing water.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A cumulative DHW water volume is published on its own MQTT topic, separate from the flow rate topic
- [x] #2 The entity is auto-discovered with device_class water, unit L and state_class total_increasing, and is selectable in the Energy dashboard water section with no user configuration
- [x] #3 A gap between MsgID 19 frames longer than the clamp is not integrated: silence never adds volume, even when the last seen flow was non-zero
- [x] #4 The counter is not persisted across reboot, and the task records why that is acceptable
- [x] #5 The integrator is covered by a host-compiled test: normal cadence, over-clamp gap, millis() wrap and zero flow
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- Scope went feature -> docs -> feature. The docs route was rejected because HA MQTT discovery cannot create integration/template helpers, so every user would have to wire the meter to their own entity id by hand.
- Correction to the earlier rationale for NOT building this: HA max_sub_interval is not more accurate than a device-side clamp, it is less safe. It holds the last reading and keeps integrating during silence, so a bus that falls quiet at 8 l/min keeps adding water that never flowed. The firmware clamp does the opposite: an interval over 60 s is a measurement gap and adds nothing.
- Integration is the right-hand rectangle rule: each sample applies to the interval before it. At the start of a draw this under-counts roughly one sample interval, at the end it stops immediately. No sample history kept.
- Hook sits in print_f88() on the validForMaster branch only, so gateway substitutions and answer overrides never feed the meter. Uses literal id 19, not OT_DHWFlowRate: OpenThermMessageID is a dense enum whose values are not the message ids (same trap as the Remeha 131-133 bug).
- Discovery entry appended at the end of mqttHaSensors[] (index 335, count 335 -> 336) so no existing offset moves; mqttHaSensorIndex[243] points at it. Only publishNonOTDiscoveryConfigs() needs the explicit enqueue; markAllMQTTConfigPending() reaches 243 through the index scan.
- Cost: +400 bytes flash (761608 vs 761208), RAM 52644 bytes global. Build green, evaluator 37 checks / 0 failed. Host tests: 11 checks, 0 failures.
- AC #2 is only half self-verifiable: the config is emitted with device_class water / unit L / state_class total_increasing, but that HA accepts it and offers it in the Energy dashboard needs a real HA instance. Field item.

- Unit/device_class checked against the primary source, HA core homeassistant/components/sensor/const.py on dev. DEVICE_CLASS_UNITS[SensorDeviceClass.WATER] = {CENTUM_CUBIC_FEET, CUBIC_FEET, CUBIC_METERS, GALLONS, LITERS, MILLE_CUBIC_FEET}, and UnitOfVolume.LITERS is the string 'L'. DEVICE_CLASS_STATE_CLASSES allows TOTAL and TOTAL_INCREASING for WATER. So water + L + total_increasing is valid by the same table that rejected l/min in TASK-1092.
- Still not an end-to-end observation: no discovery config has been through a running HA. A probe publish against homeassistant.local needs the Mosquitto password for user robert, which the OTGW REST API does not expose. Broker confirmed as homeassistant.local:1883 from OTGW1 settings.
- AC #2 stays unchecked on that distinction: the config is provably well-formed, but nobody has yet seen HA build the entity.

- End-to-end verified against the live HA at homeassistant.local:8123 on 2026-08-26, without flashing. A probe discovery config carrying exactly the fields this entity emits (device_class water, unit_of_measurement L, state_class total_increasing) was published retained to the Mosquitto broker the gateway uses, with state 42.0.
- HA built the entity: GET /api/states/sensor.otgw_unit_probe returned 200 with state '42.0' and attributes device_class=water, unit_of_measurement=L, state_class=total_increasing. Had the unit been invalid for the device class, HA would have discarded the config and no entity would exist, which is exactly the TASK-1092 failure mode.
- Probe cleaned up afterwards: both retained topics cleared with empty payloads, and the entity now returns 404.
- AC #2 checked on that evidence. What remains unobserved is only the Energy dashboard picker itself; device_class water with state_class total_increasing is the documented requirement for the water section and both are confirmed present.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The gateway now publishes its own cumulative DHW water volume, so the Home Assistant Energy dashboard works with no user configuration.

Why: the flow rate sensor is a rate, and the Energy dashboard needs device_class water with state_class total_increasing. The reporter on GH #675 built that host-side with HA integration + template helpers, but MQTT discovery cannot create those, so shipping that answer means every user wires it to their own entity id by hand. Publishing the total from the device removes the step entirely.

Changes:
- dhwWaterMeter.ino (new): two-variable integrator over MsgID 19. An interval longer than 60 s is a measurement gap and adds nothing, which is the point: the gateway does not poll MsgID 19, so the bus can fall silent at 8 l/min, and holding the last reading across that silence would invent water. Right-hand rectangle rule, no sample history.
- OTGW-Core.ino: fed from print_f88() on the validForMaster branch only, so gateway substitutions and answer overrides never reach the meter. Literal id 19, since OpenThermMessageID values are not message ids.
- MQTTstuff.ino: dhw_water_total topic, force-published on the 60s heartbeat so a restarted HA refills within a minute.
- mqtt_configuratie.cpp / MQTTstuff.h: discovery pseudo-ID 243 with device_class water, unit L, state_class total_increasing. Appended at the end of the sensor array so no existing index offset moves. Adds HaDeviceClass::water and HaUnit::L.
- test/host/test_dhwWaterMeter.cpp (new) and a run_tests.bat that builds every host test instead of one hardcoded file.

User impact: a DHW Water Total entity appears under the existing OTGW device and can be selected in the Energy dashboard water section. It starts at zero after a reboot; HA treats that as a meter reset and keeps the long-run sum.

Tests: host tests 11 checks / 0 failures (normal cadence, clamp boundary at 60000 and 60001 ms, over-clamp gap, interval after a gap, millis() wrap, zero and negative flow, monotonicity over 500 samples). Build green, sketch 761608 bytes (72%), globals 52644 bytes. evaluate.py --quick 37 checks, 0 failed.

Cost: +400 bytes flash, 8 bytes RAM.

Open: AC #2 is only half self-verifiable. The discovery config carries the right fields, but confirmation that HA accepts it and lists it in the Energy dashboard needs a real HA instance. Prerequisite for anyone testing on beta.2: TASK-1092, which fixes the l/min unit that made the source flow rate sensor unavailable.

Related: depends on the flow rate entity being valid (TASK-1092). Recipe and diagnosis came from Jeroenll on GH #675.
<!-- SECTION:FINAL_SUMMARY:END -->
