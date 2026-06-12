---
id: TASK-846
title: >-
  feat-2.0.0: combo Phase 2 - runtime-gate PIC/OTDirect mutual-exclusion
  branches
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-08 22:23'
updated_date: '2026-06-08 22:26'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-125 Phase 2. Convert compile-time #if HAS_PIC/#else mutual-exclusion branches to runtime if(isPICEnabled())/if(isOTDirectEnabled()) so the combo binary routes correctly in whichever mode it boots. Analysis identified 4 dangerous sites; the many additive #if HAS_DIRECT_OT blocks are safe and stay.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 OTGW-Core.ino handlePICSerial(): #if !HAS_PIC early-return -> runtime if(!isPICEnabled())
- [ ] #2 MQTTstuff.ino otgw32 cmd: gate handler on isOTDirectEnabled() (nested in #if HAS_DIRECT_OT)
- [ ] #3 handleDebug.ino case 'a': gate getpicfwversion() on isPICEnabled() (nested in #if HAS_PIC)
- [ ] #4 OLED.ino splash OTGW32 vs OTGW: branch on state.hw.eMode at runtime
- [ ] #5 esp32-combo + esp8266 + esp32 builds green; evaluator no new failures
- [ ] #6 Field-validated: combo in each mode drives correct OT path
<!-- AC:END -->
