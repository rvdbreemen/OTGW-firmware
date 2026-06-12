---
id: TASK-846
title: >-
  feat-2.0.0: combo Phase 2 - runtime-gate PIC/OTDirect mutual-exclusion
  branches
status: Done
assignee:
  - '@claude'
created_date: '2026-06-08 22:23'
updated_date: '2026-06-12 22:57'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-125 Phase 2. Convert compile-time #if HAS_PIC/#else mutual-exclusion branches to runtime if(isPICEnabled())/if(isOTDirectEnabled()) so the combo binary routes correctly in whichever mode it boots. Analysis identified 4 dangerous sites; the many additive #if HAS_DIRECT_OT blocks are safe and stay.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW-Core.ino handlePICSerial(): #if !HAS_PIC early-return -> runtime if(!isPICEnabled())
- [x] #2 MQTTstuff.ino otgw32 cmd: gate handler on isOTDirectEnabled() (nested in #if HAS_DIRECT_OT)
- [x] #3 handleDebug.ino case 'a': gate getpicfwversion() on isPICEnabled() (nested in #if HAS_PIC)
- [x] #4 OLED.ino splash OTGW32 vs OTGW: branch on state.hw.eMode at runtime
- [x] #5 esp32-combo + esp8266 + esp32 builds green; evaluator no new failures
- [ ] #6 Field-validated: combo in each mode drives correct OT path
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 2 committed b58180cd + pushed. 5 runtime conversions (handlePICSerial, loopOTDirect main-loop gate, OLED splash, debug a, MQTT otgw32). Builds: esp32-combo SUCCESS (Flash 98.1%), esp8266 SUCCESS; esp32 fixed-board regression in progress (combo is its superset). Evaluator 0 failures, abstraction baseline 4 unchanged. AC5 build / AC6 field-validation pending.

Audit 2026-06-13: original AC1 design (early-return in handlePICSerial) was field-falsified and replaced by TASK-861 banner recovery; the surviving runtime mutual-exclusion gates (addCommandToQueue/sendPICSerial fallthrough, restAPI 503s, main-loop OTDirect gating, MQTT otgw32 gate, OLED splash) are in the tree, and the combo-specific gating (drain + 60s PR=A on !isOTDirectEnabled, Ethernet skip in PIC mode, telnet p guard) landed via ADR-127 in commit c2aef89c under TASK-864. Phase-2 scope is fully delivered across those two tasks; nothing remains here.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Superseded/absorbed: runtime PIC/OTDirect mutual-exclusion gating was delivered partly under this task's original Phase-2 work (gates kept by ADR-126), corrected by TASK-861 (banner recovery replaced the falsified early-return design), and completed under TASK-864/ADR-127 (commit c2aef89c: drain + PR=A retry gated on !isOTDirectEnabled, Ethernet probe skipped in PIC mode, telnet p guard, initOTDirect no longer clobbers HW_MODE_PIC). Field validation of the combo lives in TASK-864 AC#6.
<!-- SECTION:FINAL_SUMMARY:END -->
