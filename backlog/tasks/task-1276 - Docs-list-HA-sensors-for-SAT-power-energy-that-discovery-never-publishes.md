---
id: TASK-1276
title: Docs list HA sensors for SAT power/energy that discovery never publishes
status: Done
assignee:
  - '@claude'
created_date: 2026-10-20 20:34
updated_date: '2026-10-22T02:10:00Z'
labels:
  - docs
  - mqtt
  - ha
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 406000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in the TASK-1258 review: docs/manuals/en/ch05-sat-thermostat.md ~602 and nl/h05 ~647 list sensor.otgw_sat_power (and related SAT energy sensors) as Home Assistant entities, but MQTTHaDiscovery.cpp has no discovery entry for sat/power, sat/energy_total or sat/energy_estimated_kwh (grep 2026-10-20). Either add discovery entities (power kW measurement, energy kWh total_increasing for the HA Energy dashboard) or correct the docs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Decision recorded: add discovery entities or correct the docs
- [x] #2 The chosen side matches the other (discovery payload capture or doc diff)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 decision (main session): add HA discovery for the SAT power and energy topics rather than trimming the docs: sat/power as power kW (state_class measurement) and the energy totals as energy kWh (state_class total_increasing) so they can feed the HA Energy dashboard; the docs already promise them.

2026-10-22 RED broker capture (alpha.449+844dff4 build on the bench, broker 192.168.88.32): homeassistant/status online -> 120 homeassistant/sensor/otgw-1020BA21B4F8/* configs, none with a stat_t ending in sat/power, sat/energy_total or sat/energy_estimated_kwh. Evidence %TEMP%\bench-1276\red-disc.txt.

2026-10-22 GREEN broker capture (alpha.450+b84a0f4 build of this diff, OTGW32 .88.61): homeassistant/status online -> homeassistant/sensor/otgw-1020BA21B4F8/sat_sat_power/config 'SAT Power' stat_t .../sat/power, power kW measurement; .../sat_sat_energy_total/config 'SAT Energy Total' stat_t .../sat/energy_total, energy kWh total_increasing; .../sat_sat_energy_estimated_kwh/config 'SAT Estimated Gas Input' stat_t .../sat/energy_estimated_kwh, energy kWh total_increasing (red: none of the 120 sensor configs had these). Tests: tests/test_sat_energy_discovery.py 5 OK (3 failures + 1 error on HEAD; index mutant red), test/host/test_dhw_water_meter.py PASS after updating its hard-coded row count 390 -> 393, evaluate --quick 72/0. C4 integration-layer doc row count updated. Pre-existing unrelated failure of test/host/test_ha_discovery_json.py filed as TASK-1287.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT power and energy now have Home Assistant entities: SAT Power (kW, measurement), SAT Energy Total and SAT Estimated Gas Input (kWh, total_increasing), so they can feed the HA Energy dashboard; the docs already promised them. AC1: decision recorded (add discovery, not trim docs). AC2: broker capture red (no configs) vs green (three configs with the right stat_t/device_class/unit/state_class); source test red on HEAD. Note: the estimated gas entity reads unknown until 'Boiler rated input' is set (it is only published then). Not verified in a real Home Assistant instance.
<!-- SECTION:FINAL_SUMMARY:END -->
