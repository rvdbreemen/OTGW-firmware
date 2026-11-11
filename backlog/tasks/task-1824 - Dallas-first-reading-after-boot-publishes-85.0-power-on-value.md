---
id: TASK-1824
title: 'Dallas: first reading after boot publishes 85.0 power-on value'
status: To Do
assignee: []
created_date: '2026-11-11T21:39:00Z'
labels:
  - sensors
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 908000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while answering thaghostnl in #nederlandse-ondersteuning (msg https://discord.com/channels/812969634638725140/815561033036333076/1570190651684036729, 'telkens een brown-out 85 graden'; answer 1570191069813936168). Own code finding, a bug. initSensors() sets sensors.setWaitForConversion(false) (sensors_ext.ino:161). pollSensors() calls sensors.requestTemperatures() (sensors_ext.ino:233) and then sensors.getTempC() in the same tick (sensors_ext.ino:264), so every read returns the PREVIOUS conversion. On the first poll after boot no conversion has completed yet and the scratchpad still holds the DS18B20 power-on value 85.0; it is stored in DallasrealDevice[i].tempC and published to MQTT and the UI. Only DEVICE_DISCONNECTED_C (-127) is filtered (sensors_ext.ino:265-269). Parasite-mode behaviour (DallasTemperature.cpp:103-104, :470) stays open until thaghostnl answers about his wiring; out of scope here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test or bench capture shows 85.0 as the first value after boot on the old code
- [ ] #2 The fix publishes no value before a completed conversion (for example skip the first read after requestTemperatures, or read in the next tick); the same test is green
- [ ] #3 No valid measurement is lost after that: a real 85.0 after a valid previous value is still published, unless it jumps
<!-- AC:END -->
