---
id: TASK-1179
title: The 60 s PR=A probe cannot re-enable a PIC that boot detection missed
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-30 10:12'
updated_date: '2026-09-30 10:43'
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
- [x] #1 The 60 s probe comment and the TASK-1175 notes describe the new recovery path
- [ ] #2 Old-vs-fix proof: on the Classic-S3 with the boot probe forced to miss (or a host harness that compiles the real consumer and dispatch code), OLD stays degraded after the PR=A reply and FIX re-enables the PIC and publishes otgw-pic/*
- [x] #3 The change ships in one commit with its own prerelease bump; build.bat (esp32-combo, esp32-classic, esp32) SUCCESS with fresh binaries; evaluate.py shows no new FAIL
- [x] #4 A PIC banner seen through the firmware callback while state.pic.bAvailable is false re-enables the PIC from the loop-side consumer (bAvailable = true, state.hw.eMode = HW_MODE_PIC) before the version publish, and does not persist a board mode (see TASK-1180)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 from TASK-1175 follow-up #1 after confirming the PR=A reply format in the PIC source and the processOT dispatch order (buf[2]==':' branch before the OTGW_BANNER branch).

2026-09-30 fix (alpha.386).
- applyPICBannerInfo() (OTGW-Core.ino): if state.pic.bAvailable is false, set it true and state.hw.eMode = HW_MODE_PIC before the strlcpy calls and sendMQTTversioninfo(). This matches 1.x TASK-1126, which also covers the diagnose ('Opentherm gateway diagnostics') and interface firmware whose banners processOT()'s case-sensitive OTGW_BANNER match never sees. No board mode is persisted (processOT's persist is wrong for the Pro variant, TASK-1180).
- The 60 s probe comment in OTGW-firmware.ino and the TASK-1175 notes describe the recovery path (AC#1).
- Host proof test/host/test_pic_banner_recovery.py: slices the REAL applyPICBannerInfo() (OTGW-Core.ino) and isPICEnabled() (OTGW-firmware.h) and runs them from a DEGRADED state (bAvailable false, eMode DEGRADED, device id 'unknown'). OLD (--rev HEAD 63f22a5e1): FAIL 4 of 6: bAvailable stays false, eMode stays DEGRADED, the device id is filled (so the probe stops) and the publish sees isPICEnabled() == false (otgw-pic/* skipped). FIX: PASS 6 of 6, including the publish seeing the PIC enabled.
- AC#2 left open on purpose: the harness compiles the consumer, not processOT()'s dispatch. The routing premise (the PIC answers PR=A with 'PR: A=OpenTherm Gateway x.x', gateway.asm:5434-5435; processOT sends buf[2]==':' lines to handlePRresponse() before its OTGW_BANNER branch) is established by reading the code, not by a compiled test or the Classic bench.
- AC#3: bin/bump-prerelease.sh alpha.385 -> alpha.386 in this commit; build.bat --target all: esp32, esp32-classic, esp32-combo SUCCESS for firmware and filesystem (fresh 12:35-12:41, alpha.386+63f22a5, images under %LOCALAPPDATA%/OTGW-capture/img-alpha386); evaluate.py --quick 70 passed / 0 / 0.
<!-- SECTION:NOTES:END -->
