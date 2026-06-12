---
id: TASK-847
title: 'feat-2.0.0: combo Phase 3 - runtime Home Assistant hardware identity'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 22:23'
updated_date: '2026-06-08 22:45'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-125 Phase 3. Make published hardware identity follow the boot-detected mode on the combo board so a combo in PIC mode advertises otgw-classic and in OTDirect mode advertises otgw32. Must not change emitted JSON on the two fixed boards (byte-identical).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 hardwareTypeName() runtime on combo (HAS_RUNTIME_HW_DETECT): HW_MODE_PIC->otgw-classic else otgw32; fixed boards keep F(HW_TYPE_NAME)
- [ ] #2 MQTT hardware_type + REST hardware_type auto-correct via hardwareTypeName() (verify timing)
- [ ] #3 HA OtCore device suffix/short-name (-pic/-ot-direct) threaded through HaDiscoveryContext from runtime eMode (avoids orphaned HA device)
- [ ] #4 Fixed-board discovery JSON byte-identical before/after
- [ ] #5 esp32-combo build green; evaluator no new failures
- [ ] #6 Field-validated: HA shows correct device identity per mode
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 3 split into Part A (done) + Part B (deferred, documented).

PART A — hardwareTypeName() runtime (DONE): on combo (HAS_RUNTIME_HW_DETECT) returns otgw-classic when HW_MODE_PIC else otgw32; fixed boards keep F(HW_TYPE_NAME). This auto-corrects the retained MQTT hardware_type topic, REST /api/v2/device/info hardware_type, and the web-UI pic-only controls (all consume hardwareTypeName()). The HA device DISPLAY name/model is already runtime via the existing isPICEnabled() meta (buildAllHaDeviceMeta), so HA shows 'OT-Core OTDirect' vs 'OT-Core PIC' correctly. AC1/AC2 satisfied; AC4 (fixed-board byte-identical) holds by construction (#else unchanged).

PART B — OtCore device IDENTIFIER suffix/short-name threading (DEFERRED): haDeviceSuffix(OtCore) returns compile-time HA_OTCORE_SUFFIX ('-pic' on combo since HAS_PIC=1) and haDeviceShortName returns HA_OTCORE_NAME ('pic'). To make the internal device identifier follow runtime mode: add otCoreSuffix/otCoreName to HaDiscoveryContext (MQTTstuff.h), populate in buildDiscoveryContext() from state.hw.eMode under HAS_RUNTIME_HW_DETECT (default to the macros on fixed boards = byte-identical), thread &ctx into haDeviceSuffix() at its 11 call sites in MQTTHaDiscovery.cpp, and fix the static kHaDeviceSuffixes[] (MQTTstuff.ino:165) + the topology-clear mirror (MQTTHaDiscovery.cpp:3817). 
Rationale for deferral: the suffix is INTERNAL (HA dedup key / topic segment), NOT user-visible (display name already correct via Part A path). It is compile-time-STABLE on combo ('-pic' always), so the agent's 'orphaned HA device' concern is a misread — it never flips between boots. Part B is internal-consistency hygiene with an 11-call-site spread and a byte-identical-fixed-board-JSON risk that warrants its own focused pass + dedicated HA-discovery field test, not a rushed bolt-on. AC3 left unchecked.
<!-- SECTION:NOTES:END -->
