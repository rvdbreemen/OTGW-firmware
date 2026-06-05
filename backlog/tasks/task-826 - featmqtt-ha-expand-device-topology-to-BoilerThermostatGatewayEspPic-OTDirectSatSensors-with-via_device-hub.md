---
id: TASK-826
title: >-
  feat(mqtt-ha): expand device topology to
  {Boiler,Thermostat,Gateway,Esp,Pic|OTDirect,Sat,Sensors} with via_device hub
status: To Do
assignee: []
created_date: '2026-06-05 21:50'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
NEW maintainer decision (2026-06-05, supersedes TASK-648 Task 4 which put Dallas on the Esp device). Expand HaDevice from {Boiler,Thermostat,Gateway,Esp,Sat} to {Boiler,Thermostat,Gateway,Esp,Pic,Sat,Sensors} (7 devices). Pic device named per hardware: 'pic' on HAS_PIC, 'otdirect' on HAS_DIRECT_OT (OTGW32); carries PIC firmware version/device-id/type (PIC) resp. OT-Direct mode/status. Sensors device carries the Dallas 1-wire temp sensors + the S0 pulse counter (re-homed from Esp/Gateway). via_device: Gateway is the hub - Boiler/Thermostat/Esp/Pic/Sat/Sensors all emit via_device -> <nodeId>-gateway so HA nests them under the Gateway.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 HaDevice enum (MQTTstuff.h) = {Boiler,Thermostat,Gateway,Esp,Pic,Sat,Sensors}; devMeta[]/deviceIntroduced[] arrays resized 5->7 and all '< 5' index guards updated to '< 7'
- [ ] #2 haDeviceSuffix + the two name switches in MQTTHaDiscovery.cpp handle Pic (platform-conditional 'pic'/'otdirect') and Sensors ('sensors')
- [ ] #3 writeDeviceBlock emits via_device:<nodeId>-gateway for every non-Gateway device (modern/non-legacy path only); legacy path byte-identical
- [ ] #4 Dallas sensor discovery uses HaDevice::Sensors (was Esp); S0 pulse-counter discovery uses HaDevice::Sensors
- [ ] #5 PIC firmware-info entities use HaDevice::Pic on HAS_PIC; OT-Direct status entities use HaDevice::Pic (named otdirect) on HAS_DIRECT_OT
- [ ] #6 devMeta for Pic + Sensors populated in MQTTstuff.ino (name/model/identifiers/sw-hw)
- [ ] #7 Golden-file discovery test (tests/ per ADR-122) updated for the 7-device topology + via_device; build esp32+esp8266 green; evaluate.py --quick no new failures
- [ ] #8 ADR written: new device-topology decision, supersedes the relevant part of TASK-648/ADR-077; legacy single-device mode unchanged
<!-- AC:END -->
