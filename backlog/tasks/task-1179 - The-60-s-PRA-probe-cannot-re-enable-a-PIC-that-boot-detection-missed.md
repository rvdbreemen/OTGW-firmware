---
id: TASK-1179
title: The 60 s PR=A probe cannot re-enable a PIC that boot detection missed
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 10:12'
updated_date: '2026-09-30 12:39'
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
- [x] #2 Old-vs-fix proof: on the Classic-S3 with the boot probe forced to miss (or a host harness that compiles the real consumer and dispatch code), OLD stays degraded after the PR=A reply and FIX re-enables the PIC and publishes otgw-pic/*
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

AC#2 closed 2026-09-30 with the host-harness route the AC allows: test/host/test_pic_banner_dispatch.py compiles the real byte-level dispatch, not only the consumer. Sliced by anchor: OTGWSerial::matchBanner(), registerFirmwareCallback(), firmwareVersion(), the banner table and file statics (OTGWSerial library); fwreportinfo(), the PIC-task flag block, applyPICBannerInfo() and reportPendingPICRxErrors() (OTGW-Core.ino); isPICEnabled() (OTGW-firmware.h). The PR=A reply 'PR: A=OpenTherm Gateway 6.6
' is fed byte by byte to matchBanner(), as OTGWSerial::read() does, then the loop consumer runs.
Run: python test/host/test_pic_banner_dispatch.py --old-rev 63f22a5e1 (the commit before this fix). FIX 4/4. OLD fails exactly D1 and D3: after the PR=A reply (D1) and after the diagnostics banner (D3) OLD stays available=0 / DEGRADED and runs the publish with isPICEnabled() false; FIX re-enables (available=1, mode PIC) before the publish. Controls D2 (a PR reply without a banner) and D4 (PIC found at boot) pass on both. RESULT: PASS. Transcript: %LOCALAPPDATA%/OTGW-capture/a1-patches/TASK-1179-dispatch-oldvsfix-63f22a5e1.txt
The publish is a recording double; the real sendMQTTversioninfo() (MQTTstuff.ino) sends otgw-pic/version, deviceid, firmwaretype and designer only inside if (isPICEnabled()), so 'enabled at publish' is exactly 'otgw-pic/* published' (read in code, second source). Not run on the Classic-S3: only the OTGW32 is connected.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A PIC that detectPIC() missed at boot is now re-enabled by the next firmware banner, including the PR=A reply of the 60 s probe: applyPICBannerInfo() sets bAvailable and HW_MODE_PIC before the version publish, which otherwise skipped otgw-pic/* (c459331e3, alpha.386; follow-ups TASK-1180/1181).
Evidence per AC:
- AC#1: probe comment and TASK-1175 notes describe the path (c459331e3).
- AC#2: test/host/test_pic_banner_dispatch.py compiles the real matchBanner -> fwreportinfo -> flag -> reportPendingPICRxErrors -> applyPICBannerInfo chain; OLD (63f22a5e1) stays DEGRADED after the PR=A reply and publishes with isPICEnabled() false (otgw-pic/* skipped), FIX re-enables first; controls unchanged.
- AC#3: alpha.386 built for the three targets, evaluate green (earlier notes).
- AC#4: test/host/test_pic_banner_recovery.py plus the AC#2 harness.
No Classic-S3 bench run: that board is not connected; the AC allows the harness route.
<!-- SECTION:FINAL_SUMMARY:END -->
