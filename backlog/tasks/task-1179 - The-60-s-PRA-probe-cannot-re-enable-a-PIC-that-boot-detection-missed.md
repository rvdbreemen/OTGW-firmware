---
id: TASK-1179
title: The 60 s PR=A probe cannot re-enable a PIC that boot detection missed
status: To Do
assignee: []
created_date: '2026-09-30 10:12'
labels:
  - bug
  - pic
  - recovery
dependencies: []
priority: medium
ordinal: 302000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev / 2.0.0, PIC boards; code reading, not bench-verified)
When detectPIC() misses the PIC at boot, OTGW-firmware.ino's 60 s probe sends PR=A through the PIC task's TX queue, meant as 'the only automatic path to re-detect a real PIC and re-enable all PIC functions'. The PIC answers 'PR: A=OpenTherm Gateway x.x' (other-projects/otgw-6.6/gateway.asm:5434-5435). processOT() routes that line through the buf[2]==':' branch to handlePRresponse(), which ignores register A, so processOT()'s banner branch, the only place that sets state.pic.bAvailable = true and state.hw.eMode = HW_MODE_PIC at runtime, is never reached for it. Meanwhile the firmware banner callback (applied loop-side by applyPICBannerInfo(), TASK-1175) fills state.pic.sDeviceid, which ends the probe. Net effect: after a boot-time miss the PIC stays disabled (degraded) until the next reboot or an unsolicited banner.
1.x solved this in TASK-1126 by self-healing bAvailable in the banner callback. On dev that logic belongs in the loop-side consumer applyPICBannerInfo() (TASK-1175 follow-up #1). Ordering note from TASK-1175: the consumer runs before processOT's recovery, so a recovery inside the consumer must happen before its sendMQTTversioninfo() call, or the otgw-pic/* topics are skipped (isPICEnabled() false).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A PIC banner seen through the firmware callback while state.pic.bAvailable is false re-enables the PIC the same way processOT()'s banner branch does (bAvailable, HW_MODE_PIC, and on the combo board the persisted board mode), from the loop-side consumer, before the version publish
- [ ] #2 The 60 s probe comment and the TASK-1175 notes describe the new recovery path
- [ ] #3 Old-vs-fix proof: on the Classic-S3 with the boot probe forced to miss (or a host harness that compiles the real consumer and dispatch code), OLD stays degraded after the PR=A reply and FIX re-enables the PIC and publishes otgw-pic/*
- [ ] #4 The change ships in one commit with its own prerelease bump; build.bat (esp32-combo, esp32-classic, esp32) SUCCESS with fresh binaries; evaluate.py shows no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from TASK-1175 follow-up #1 after confirming the PR=A reply format in the PIC source and the processOT dispatch order (buf[2]==':' branch before the OTGW_BANNER branch).
<!-- SECTION:NOTES:END -->
