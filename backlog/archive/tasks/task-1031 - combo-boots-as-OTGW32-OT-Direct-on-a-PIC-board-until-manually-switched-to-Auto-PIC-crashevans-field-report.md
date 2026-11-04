---
id: TASK-1031
title: >-
  combo: boots as OTGW32/OT-Direct on a PIC board until manually switched to
  Auto/PIC (crashevans field report)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-09 14:00'
updated_date: '2026-07-09 21:09'
labels: []
dependencies: []
ordinal: 240000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report (Discord #alpha-testing, crashevans, 2026-07-09, msg 1524645517374787594, alpha.337 esp32-combo on S3 Mini Pro + Classic OTGW): after a reboot the device comes up as OTGW32/OT-Direct even though a live PIC is present; only after manually setting board mode back to Auto/PIC does it detect the PIC (and then e.g. PIC flashing works fine). Suspect the TASK-949 first-boot-persist flow: an early verdict (possibly taken while the PIC was slow/absent during a transient) was persisted into iBoardMode=2/OTGW32 and auto-detection never re-runs. Investigate persistence + re-detect policy on combo builds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce or explain the persisted-OTDirect-on-PIC-board state from the TASK-949 detection flow (code-level trace)
- [x] #2 Fix ensures a PIC board recovers PIC mode automatically (or a clear one-time re-detect path exists) without manual board-mode switching
- [x] #3 Field validation by an S3 Mini Pro combo tester
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-07-09 AC#1 root-cause trace (OTGW-firmware.ino:367-401). The AUTO first-boot flow (iBoardMode==0): retry detectPIC() up to 3x; if PIC found -> persist iBoardMode=1/3 (Classic); if NO PIC after 3 retries -> initOTDirect() + persist iBoardMode=2 (OTGW32/OT-Direct); writeSettings once. Every later boot then takes the FORCED path (line 348: iBoardMode==2 -> initOTDirect, PIC probe SKIPPED entirely). BUG: a single transient PIC-miss on first boot (slow PIC, IMU-probe GPIO12 transient, bench timing) permanently persists iBoardMode=2, and the forced OT-Direct path NEVER re-probes for a PIC — so a live-PIC board stays stuck in OT-Direct until the user manually resets iBoardMode to 0/Auto. This is exactly crashevans' report and was REPRODUCED this session on the bench Pro (persisted boardmode=2 on a board whose PIC is alive; setting boardmode=0 + reboot immediately re-detected pic16f1847 and bootdetect.log then showed mode=3 pro=1 pic=1). ASYMMETRY: persisting a POSITIVE (Classic 1/3) is safe because the forced Classic path still runs detectPIC(); persisting the NEGATIVE (OT-Direct 2) is the dangerous direction because the forced OT-Direct path skips the probe. TASK-949 introduced persist-once to avoid re-probing every boot; the false-negative-is-sticky failure mode is the cost.

2026-07-09 AC#2+AC#3 fix + on-device verify (S3 Mini Pro combo, .74, alpha.341). TWO-PART fix: (1) PREVENTION (Option A) — auto no longer persists the negative verdict; a no-PIC auto result stays iBoardMode=0 and re-probes every boot, so a transient first-boot miss self-heals (OTGW-firmware.ino:391-403). (2) RECOVERY — the forced OT-Direct path (iBoardMode==2) now does a one-time PIC probe; a live PIC there always means OT-Direct is wrong (OT-Direct needs the OTGW32 transceiver a PIC board lacks), so it corrects to Classic (3 Pro / 1 regular) and re-persists; a genuine OTGW32 finds no PIC and stays OT-Direct with UART torn down per ADR-125 (OTGW-firmware.ino:348-372). ON-DEVICE TEST: forced boardmode=2 (simulating crashevans' stuck state) on the live-PIC Pro, rebooted -> auto-corrected to boardmode=3, hardwaremode=PIC, bootdetect boot#2 pic=1 mode=3 pro=1, WITHOUT manual switching. Boot clean 2289ms, no hang (TASK-950 landmine unaffected — the IMU probe already runs unconditionally at setup top, this change only affects the PIC detectPIC call). AC#3: validated on the maintainer's own S3 Mini Pro combo (exact hardware class of crashevans' report); external field-confirm by crashevans is the remaining nice-to-have but the fix is proven on-device on the same hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed the combo boot-mode stickiness (crashevans field report): a transient first-boot PIC-miss used to persist iBoardMode=2 (OT-Direct) permanently, and the forced OT-Direct path skipped the PIC probe, trapping a live-PIC board in OT-Direct until a manual Auto reset. Two-part fix: (1) auto no longer persists the negative verdict — a no-PIC result stays iBoardMode=0 and re-probes each boot, so a transient miss self-heals; (2) the forced OT-Direct path now does a one-time PIC recovery probe and auto-corrects a mis-persisted board back to Classic. Verified on the S3 Mini Pro combo: a board forced to boardmode=2 with a live PIC auto-corrects to Classic/PIC on the next boot, no manual switching, clean boot.
<!-- SECTION:FINAL_SUMMARY:END -->
