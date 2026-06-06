---
id: TASK-826
title: >-
  feat(mqtt-ha): expand device topology to
  {Boiler,Thermostat,Gateway,Esp,Pic|OTDirect,Sat,Sensors} with via_device hub
status: Done
assignee:
  - '@claude'
created_date: '2026-06-05 21:50'
updated_date: '2026-06-06 06:14'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
NEW maintainer decision (2026-06-05, supersedes TASK-648 Task 4 which put Dallas on the Esp device). Expand HaDevice from {Boiler,Thermostat,Gateway,Esp,Sat} to {Boiler,Thermostat,Gateway,Esp,Pic,Sat,Sensors} (7 devices). Pic device named per hardware: 'pic' on HAS_PIC, 'otdirect' on HAS_DIRECT_OT (OTGW32); carries PIC firmware version/device-id/type (PIC) resp. OT-Direct mode/status. Sensors device carries the Dallas 1-wire temp sensors + the S0 pulse counter (re-homed from Esp/Gateway). via_device: Gateway is the hub - Boiler/Thermostat/Esp/Pic/Sat/Sensors all emit via_device -> <nodeId>-gateway so HA nests them under the Gateway.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 HaDevice enum (MQTTstuff.h) = {Boiler,Thermostat,Gateway,Esp,Pic,Sat,Sensors}; devMeta[]/deviceIntroduced[] arrays resized 5->7 and all '< 5' index guards updated to '< 7'
- [x] #2 haDeviceSuffix + the two name switches in MQTTHaDiscovery.cpp handle Pic (platform-conditional 'pic'/'otdirect') and Sensors ('sensors')
- [x] #3 writeDeviceBlock emits via_device:<nodeId>-gateway for every non-Gateway device (modern/non-legacy path only); legacy path byte-identical
- [x] #4 Dallas sensor discovery uses HaDevice::Sensors (was Esp); S0 pulse-counter discovery uses HaDevice::Sensors
- [x] #5 PIC firmware-info entities use HaDevice::Pic on HAS_PIC; OT-Direct status entities use HaDevice::Pic (named otdirect) on HAS_DIRECT_OT
- [x] #6 devMeta for Pic + Sensors populated in MQTTstuff.ino (name/model/identifiers/sw-hw)
- [x] #7 Golden-file discovery test (tests/ per ADR-122) updated for the 7-device topology + via_device; build esp32+esp8266 green; evaluate.py --quick no new failures
- [x] #8 ADR written: new device-topology decision, supersedes the relevant part of TASK-648/ADR-077; legacy single-device mode unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
@
## Implementation plan (TASK-826: 7-device HA topology)

Foundation: ADR-124 (Proposed) locks design. 648 topology commits landed; target files clean.

### Enum reorder (NOT just grow): Sat moves index 4->5
MQTTstuff.h:366  enum HaDevice = { Boiler=0, Thermostat, Gateway, Esp, Pic, Sat, Sensors }
Comments "5 entries" -> "7" (h:400,404).

### Arrays resize+reorder [5]->[7] in lockstep
MQTTstuff.ino:142 g_haDeviceIntroduced; 157-158 g_haDeviceMetaBufs/g_haDeviceMeta.
All `< 5` guards -> `< 7` (ino:2296,2300,2364,2457; cpp:2282). kSuffixes[] grow+reorder.

### Routing (two mirrored fns)
deviceForOTId (ino:2619) + topoDeviceForPseudoId (cpp:3810):
  245 (S0) -> Sensors (currently default Esp; ADD case)
  246 (dallas) -> Sensors (was Esp)
  249 (picinfo) -> Pic (was Gateway)
Update comment block ino:2596-2609. Dallas path (streamDallasSensorDiscovery cpp:2733) -> Sensors.

### Names/suffix (1 suffix switch + 2 name switches)
haDeviceSuffix (cpp:2220): +Pic (-pic/-otdirect via HAS_PIC/HAS_DIRECT_OT), +Sensors (-sensors).
Name switches cpp:2580 + cpp:3798: +Pic (pic/otdirect), +Sensors (sensors). Flag-gated, no raw ifdef.

### via_device (both emitters)
writeDeviceBlock (cpp:2241) + buildDiscoveryDeviceBlock (ino:2269): emit
"via_device":"<nodeId>-gateway" for every non-Gateway device, modern path only. Legacy byte-identical.

### Metadata
buildAllHaDeviceMeta: populate Pic (name/model/sw=PIC fw or OTDirect, identifiers <nodeId>-pic|-otdirect) + Sensors (<nodeId>-sensors).

### Verify
build esp32 + esp8266 (grep both SUCCESS lines; build.py masks per-env fail);
evaluate.py --quick; golden discovery test updated for 7 devices + via_device.
ADR-124 Proposed->Accepted after green.

### OPEN FORK (AC#5 OTDirect half) - needs user decision
On OTGW32 (HAS_DIRECT_OT) 249 self-skips; only OTDirect entities are 2x otdirect_flame_* bundled in
pseudo-ID 251 WITH SAT BLE health. Coarse routing cannot split them. Options A/B/C presented to user.
@
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Design fork resolved (user, 2026-06-06): Option C. New pseudo-ID 243 (otdirect): otdirect_flame_duty_pct + otdirect_flame_cycles_per_hr move OUT of 251 into 243; 243->Pic (HAS_DIRECT_OT), 251->Sat (SAT BLE health only). Fulfills AC#5 fully.

Implemented + committed locally (563ceaf, alpha.164). Both targets build green (esp32+esp8266, fw+fs, 0 FAILED); evaluate.py --quick green (abstraction baseline unchanged at 4 — HAS_PIC is sanctioned). ADR-124 Accepted. Naming per maintainer: enum HaDevice::OtCore (code), user-facing suffix per hardware 'pic'/'ot-direct' so the model is visible in HA; display name 'OT-Core PIC/OTDirect (host)'. NOT pushed, NOT marked Done — two open items: (1) UPGRADE ORPHAN: re-homing Dallas/S0 (esp->sensors), picinfo (gateway->ot-core), flame (sat->ot-core via 243) changes the discovery CONFIG TOPIC (homeassistant/<comp>/<nodeId>/<deviceseg>_<label>/config), so alpha.163->164 upgraders keep stale retained configs = duplicate HA entities. Existing armTopologyCleanup only covers legacy<->modern axis, not modern->modern re-home. Needs maintainer decision: ship+follow-up vs add one-shot cleanup now. (2) Golden test is dump-driven; seven-device/via_device output unverified without a captured MODERN_DUMP (field validation). Documented honestly in ADR-124.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Seven-device HA discovery topology (ADR-124) shipped in alpha.164. HaDevice enum 5->7 {Boiler,Thermostat,Gateway,Esp,OtCore,Sat,Sensors}; per-device arrays/guards scale via HA_DEVICE_COUNT; three duplicate suffix tables collapsed into kHaDeviceSuffixes[]. OT-Core is one code enum (HaDevice::OtCore) rendered per hardware in HA for model visibility: 'pic'/-pic (HAS_PIC) vs 'ot-direct'/-ot-direct (HAS_DIRECT_OT), name 'OT-Core PIC/OTDirect (host)'. Routing: Dallas+S0->Sensors, picinfo(249)->OtCore, OTDirect flame metrics split from pseudo-ID 251 into new 243->OtCore (armed alongside 251). via_device:<nodeId>-gateway emitted by both device-block builders (modern only; legacy byte-identical). HAS_PIC reaches the standalone .cpp TU via boards.h in MQTTstuff.h; no raw platform ifdef (abstraction baseline unchanged). Golden test updated for 7 devices + inverted via_device assertion. esp32+esp8266 build green (fw+fs, 0 FAILED); evaluate.py --quick green. KNOWN (accepted by maintainer, alpha phase): re-homed entities change discovery config topic -> 163->164 upgraders get duplicate HA entities until old retained configs are cleared MANUALLY; no migration code by decision. Gate is dump-driven field-validation (documented honestly in ADR-124); no compile-time evaluate.py gate yet (follow-up).
<!-- SECTION:FINAL_SUMMARY:END -->
