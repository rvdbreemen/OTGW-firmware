---
id: TASK-847
title: 'feat-2.0.0: combo Phase 3 - runtime Home Assistant hardware identity'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 22:23'
updated_date: '2026-06-12 22:57'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-125 Phase 3. Make published hardware identity follow the boot-detected mode on the combo board so a combo in PIC mode advertises otgw-classic and in OTDirect mode advertises otgw32. Must not change emitted JSON on the two fixed boards (byte-identical).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 hardwareTypeName() runtime on combo (HAS_RUNTIME_HW_DETECT): HW_MODE_PIC->otgw-classic else otgw32; fixed boards keep F(HW_TYPE_NAME)
- [x] #2 MQTT hardware_type + REST hardware_type auto-correct via hardwareTypeName() (verify timing)
- [ ] #3 HA OtCore device suffix/short-name (-pic/-ot-direct) threaded through HaDiscoveryContext from runtime eMode (avoids orphaned HA device)
- [x] #4 Fixed-board discovery JSON byte-identical before/after
- [x] #5 esp32-combo build green; evaluator no new failures
- [ ] #6 Field-validated: HA shows correct device identity per mode
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 3 split into Part A (done) + Part B (deferred, documented).

PART A — hardwareTypeName() runtime (DONE): on combo (HAS_RUNTIME_HW_DETECT) returns otgw-classic when HW_MODE_PIC else otgw32; fixed boards keep F(HW_TYPE_NAME). This auto-corrects the retained MQTT hardware_type topic, REST /api/v2/device/info hardware_type, and the web-UI pic-only controls (all consume hardwareTypeName()). The HA device DISPLAY name/model is already runtime via the existing isPICEnabled() meta (buildAllHaDeviceMeta), so HA shows 'OT-Core OTDirect' vs 'OT-Core PIC' correctly. AC1/AC2 satisfied; AC4 (fixed-board byte-identical) holds by construction (#else unchanged).

PART B — OtCore device IDENTIFIER suffix/short-name threading (DEFERRED): haDeviceSuffix(OtCore) returns compile-time HA_OTCORE_SUFFIX ('-pic' on combo since HAS_PIC=1) and haDeviceShortName returns HA_OTCORE_NAME ('pic'). To make the internal device identifier follow runtime mode: add otCoreSuffix/otCoreName to HaDiscoveryContext (MQTTstuff.h), populate in buildDiscoveryContext() from state.hw.eMode under HAS_RUNTIME_HW_DETECT (default to the macros on fixed boards = byte-identical), thread &ctx into haDeviceSuffix() at its 11 call sites in MQTTHaDiscovery.cpp, and fix the static kHaDeviceSuffixes[] (MQTTstuff.ino:165) + the topology-clear mirror (MQTTHaDiscovery.cpp:3817). 
Rationale for deferral: the suffix is INTERNAL (HA dedup key / topic segment), NOT user-visible (display name already correct via Part A path). It is compile-time-STABLE on combo ('-pic' always), so the agent's 'orphaned HA device' concern is a misread — it never flips between boots. Part B is internal-consistency hygiene with an 11-call-site spread and a byte-identical-fixed-board-JSON risk that warrants its own focused pass + dedicated HA-discovery field test, not a rushed bolt-on. AC3 left unchecked.

Part A committed 77a78f3a + pushed (alpha.168). Combo build green. Remaining on this task: AC3 Part B (OtCore identifier suffix threading, deferred — see plan above), AC6 field validation. Phase 2 committed b58180cd (alpha.167), Phase 1 6dcbd395 (alpha.166).

Audit + re-scope 2026-06-13: Part A (runtime hardwareTypeName slug otgw-classic/otgw32 on the combo) was reverted by ec55a1fb (ADR-126) and has now been RE-DELIVERED under TASK-864/ADR-127, commit c2aef89c (OTGW-firmware.h, HAS_RUNTIME_HW_DETECT-gated). Remaining scope of THIS task narrows to the explicitly deferred gap in ADR-127 section 10: runtime OtCore HA device name/suffix (HA_OTCORE_NAME/SUFFIX compile-time, ~17 sites in 2 TUs incl. MQTTHaDiscovery.cpp) so a combo in OTDirect mode stops advertising OtCore as pic. Do not re-deliver Part A here; double-counting.
<!-- SECTION:NOTES:END -->
