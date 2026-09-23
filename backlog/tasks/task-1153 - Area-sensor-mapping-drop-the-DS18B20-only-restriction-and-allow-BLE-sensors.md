---
id: TASK-1153
title: 'Area sensor mapping: drop the DS18B20-only restriction and allow BLE sensors'
status: In Review
assignee:
  - '@claude'
created_date: '2026-09-22 06:38'
updated_date: '2026-09-23 18:04'
labels: []
dependencies: []
ordinal: 288000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported on Discord #dev-sat-mqtt 2026-06-21 by sergeantd, seconded by number3nl: 'the ds18b20 area sensor mapping should be renamed to just Area sensor mapping. I think it is better to include all the discovered sensors in the areas dropdown. Since we are talking for other rooms it is a bit difficult to hardwire a ds18b20, but it is pretty simple to add a ble sensor.' number3nl: 'ds18b20 is not a logical way to work here. Make it possible to connect the BLE sensors here, not the ds18b20 sensors.'

Verified against dev HEAD 2a2e5fba3 (2026-09-22). Still applicable; an adversarial pass confirmed all 19 citations verbatim.

NOTHING CHANGED SINCE THE REPORT. The last commit touching either the UI panel (buildDallasSensorAreasPanel) or the storage field (sSensorArea) is 964da4a52 of 2026-05-08, the original TASK-587 landing, six weeks BEFORE the request.

CURRENT STATE: index.js:4131 renders literally ' DS18B20 Area Sensor Mapping' and the hint at :4143 reads 'Map a discovered DS18B20 sensor to each SAT area.' The dropdown is populated from Dallas sensors only, and the persisted field holds a 16-hex Dallas address. The v2 UI is worse off: it exposes four raw free-text satsensorarea0..3 inputs with no picker at all.

WHY IT MATTERS: BLE sensors are already discovered and rostered (/api/v2/sat/ble/discovery, /api/v2/sat/ble/roster). The firmware has the data; only the mapping surface refuses it. Hardwiring a DS18B20 into another room is exactly the friction the reporters describe.

SCHEMA IMPACT: widening sSensorArea from a Dallas-only field to a multi-source sensor reference (16-hex address or 18-byte MAC) is a settings-schema change. Existing settings.json files with Dallas-only mappings must load unchanged. This wants an ADR or an ADR-051 amendment recording the widening plus the migration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The panel is titled 'Area Sensor Mapping' with no DS18B20 in title or hint, in the classic UI and in v2
- [x] #2 A BLE roster sensor can be assigned to a SAT area and its temperature reaches state.sat.fAreaTemp[N] on the normal update cadence, verified on-device with at least one BLE sensor
- [x] #3 The persisted area-sensor field accepts both a 16-hex Dallas address and an 18-byte BLE MAC; existing Dallas-only settings.json files load unchanged after upgrade
- [x] #4 GET and PATCH /api/v2/sat/sensor-areas accept and return both address forms; invalid values still 400; the GET response buffer is sized for the widest case
- [x] #5 The area dropdown is populated from all discovered sensors (Dallas via /api/v2/sensors, BLE via /api/v2/sat/ble/discovery), each showing its user label where one exists
- [x] #6 The v2 UI gets the same picker instead of the four raw free-text satsensorarea0..3 inputs
- [x] #7 Exactly one in-firmware writer per source: the Dallas poll loop and the BLE update path each call satSetAreaTemp() only for their own mapped areas
- [x] #8 Frontend stays within the N<=2 in-flight cap (ADR-165) while fetching the two discovery lists plus the current mappings
- [ ] #9 An ADR (or ADR-051 amendment) records widening sSensorArea to a multi-source sensor reference, including the migration
- [x] #10 python build.py green for the ESP32 targets; python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Shipped under alpha.369. Build green on esp32, esp32-classic and esp32-combo; evaluate.py --quick 0 failures, 98.7%.

Evidence: tests/webui/sat-area-sensor-mapping.test.mjs (new) drives the shipped classic UI in headless Chrome over CDP, serving both sensor sources plus the current mappings. 12/12 checks pass; the same harness reports 6 failures against the parent commit, including 'the BLE sensor is offered' and a PATCH that carried an empty sensor value, so it fails for the right reason.

AC2 (a BLE temperature actually reaching state.sat.fAreaTemp[N] on device) needs hardware and is NOT ticked. The fan-out is implemented in satBLEUpdateState() and mirrors the Dallas path in sensors_ext.ino:298, but no 2.0.0 board with a BLE sensor was reachable this session.

AC9 is satisfied by ADR-178 (Proposed, lints clean). It carries one Open Question for the maintainer: when an area's mapped BLE sensor goes stale, hold the last value or fall back? Area mapping deliberately has no failover - a substitute sensor in a different room would report the wrong area - but that needs confirming before the ADR can be accepted.

2026-09-23 on-device validation on OTGW32 (192.168.88.61, alpha.372, no PSRAM so satbleriskack=true for the test). BLE roster found 4 ATC sensors. PATCH /api/v2/sat/sensor-areas mapped A4:C1:38:FE:A8:7C to area 0 and A4:C1:38:96:53:CD to area 1 (both returned in GET in 17-char MAC form). On the next BLE interval tick /api/v2/debug showed state.sat.area0_temp=22.41 and area1_temp=23.99, matching the roster temps of those two MACs (22.43 / 23.99). AC2 met. AC9 stays open on the ADR-178 stale-sensor question.
<!-- SECTION:NOTES:END -->
