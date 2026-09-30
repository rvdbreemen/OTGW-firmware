---
id: TASK-1181
title: A replayed PIC banner can switch a combo OTGW32 to Classic mode and persist it
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 10:50'
updated_date: '2026-09-30 10:56'
labels:
  - bug
  - combo
  - simulation
dependencies: []
priority: medium
ordinal: 304000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
LATENT DEFECT (dev / 2.0.0 combo image on OTGW32 hardware; code reading; triage follow-up 'a replayed banner line flipping the combo board to PIC mode')
Since TASK-1071 the OT frame replay feeds fixture lines through dispatchOTGWInputLine() into processOT() on OT-Direct boards too. processOT()'s banner branch (#if HAS_PIC, so present in the combo image) matches any line containing 'OpenTherm Gateway'. On a combo board running OT-Direct (auto mode, iBoardMode 0) a replayed banner line would set state.pic.bAvailable = true and state.hw.eMode = HW_MODE_PIC on hardware without a PIC, publish PIC version info, and persist a Classic board mode (iBoardMode 1, or after TASK-1180 bClassicPro ? 3 : 1). The next boot then forces Classic mode on an OTGW32, which runs DEGRADED until the board mode is reset by hand.
The committed fixtures (scripts/tests/otgw_simulation_coverage.log, tests/otgw_simulation.log) contain no banner line, but /otgw_simulation.log is any user-uploaded log, and a real OTmonitor capture typically contains the boot banner. While OT-Direct is active the PIC UART is closed (setup(): OTGWSerial.end() before initOTDirect()), so a banner seen then cannot come from a PIC.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 processOT()'s banner branch does not re-enable the PIC, change the hardware mode or persist a board mode while OT-Direct is active (isOTDirectEnabled()); a real PIC banner on a Classic board is unaffected
- [x] #2 Old-vs-fix host proof that compiles the real banner recovery block: with OT-Direct active in auto mode, OLD sets bAvailable and persists a Classic mode, FIX changes nothing; the Classic recovery cases of TASK-1180 still pass
- [x] #3 The change ships in one commit with its own prerelease bump; build.bat esp32-combo SUCCESS with fresh binaries; evaluate.py --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 while closing TASK-1180: its 'auto mode cannot reach this branch' argument holds for real banners but not for replayed lines.

2026-09-30 fix (alpha.388). processOT()'s banner branch now only re-enables the PIC (and persists a board mode) when '!state.pic.bAvailable && !isOTDirectEnabled()': while OT-Direct runs the PIC UART is closed, so a banner line can only come from the frame replay.
Host proof test/host/test_banner_board_mode.py (slices the real recovery block after the OTGW_BANNER test; isOTDirectEnabled() is a hardware-state double):
- OLD (--rev HEAD 052c382ce): FAIL 1 of 5: 'replayed banner while OT-Direct runs' -> bAvailable 1, board mode persisted 1, one settings write.
- FIX: PASS 5 of 5; the four TASK-1180 Classic cases still pass.
Build: build.bat --target esp32-combo SUCCESS (fw + fs, fresh 12:55-12:56, alpha.388+052c382, images under %LOCALAPPDATA%/OTGW-capture/img-alpha388); evaluate.py --quick 70 passed / 0 / 0.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
On the combo image running OT-Direct (an OTGW32 in auto mode), a boot banner inside a replayed user log reached processOT()'s banner branch, which switched the PIC functions on, set HW_MODE_PIC and persisted a Classic board mode, so the next boot ran the OTGW32 as a Classic board without a PIC. The branch now skips the recovery while OT-Direct is active, because the PIC UART is closed then. Proven on the host: the old block persists a Classic mode for a replayed banner, the fixed block changes nothing, and the TASK-1180 Classic recovery cases still pass. Combo build and evaluate.py --quick are green at alpha.388.
<!-- SECTION:FINAL_SUMMARY:END -->
