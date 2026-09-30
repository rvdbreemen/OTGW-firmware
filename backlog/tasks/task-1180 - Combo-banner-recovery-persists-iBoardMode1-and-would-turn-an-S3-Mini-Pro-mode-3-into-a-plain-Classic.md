---
id: TASK-1180
title: >-
  Combo banner recovery persists iBoardMode=1 and would turn an S3 Mini Pro
  (mode 3) into a plain Classic
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 10:32'
updated_date: '2026-09-30 10:51'
labels:
  - bug
  - combo
  - pic
dependencies: []
priority: low
ordinal: 303000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
LATENT DEFECT (dev / 2.0.0 combo builds; code reading, not bench-verified; found while fixing TASK-1179)
processOT()'s banner branch, when it re-enables a PIC that boot detection missed, also runs (under HAS_RUNTIME_HW_DETECT) 'if (settings.iBoardMode != 1) { settings.iBoardMode = 1; writeSettings(false); }', commented as 'a degraded combo boot that recovers via the banner is by definition a Classic board'.
That branch can only run while the PIC UART is open and the PIC was missed: in auto mode (iBoardMode 0) a failed detection runs OTGWSerial.end() and initOTDirect(), so no banner can arrive; forced OT-Direct (2) never opens the UART. The persist therefore only triggers from a forced Classic mode: 1 (a no-op) or 3, the S3 Mini Pro variant (ADR-158), which it would silently rewrite to 1, the plain S3 Mini pin map, on the next boot.
The loop-side recovery added in TASK-1179 (applyPICBannerInfo) deliberately does not persist the board mode.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The banner recovery never writes an iBoardMode that differs from the board's detected variant: either the persist is removed (it cannot trigger from auto mode) or it writes state.hw.bClassicPro ? 3 : 1 only when iBoardMode is 0
- [x] #2 Old-vs-fix proof: a host harness that compiles the real banner branch (or a Classic S3 Mini Pro on the bench with iBoardMode 3 and a forced boot-probe miss) shows OLD rewriting 3 to 1 and FIX keeping 3
- [x] #3 The change ships in one commit with its own prerelease bump; build.bat esp32-combo SUCCESS; evaluate.py no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 during TASK-1179. Reachability argued from OTGW-firmware.ino setup(): auto-mode detection failure closes the PIC UART and starts OT-Direct.

2026-09-30 fix (alpha.387). processOT()'s banner recovery persists a board mode only from auto (iBoardMode == 0), as state.hw.bClassicPro ? 3 : 1, the same expression setup() uses; a forced Classic mode (1, or 3 for the S3 Mini Pro) is left alone.
Host proof test/host/test_banner_board_mode.py (slices the real recovery block that follows the OTGW_BANNER test in processOT(); MSVC):
- OLD (--rev HEAD c459331e3): FAIL 2 of 4: a forced S3 Mini Pro goes 3 -> 1 with a settings write; auto on a Pro learns 1 instead of 3.
- FIX: PASS 4 of 4: forced Pro keeps 3 (no write), forced S3 Mini keeps 1 (no write), auto learns 3 on a Pro and 1 on an S3 Mini (one write each).
Build: build.bat --target esp32-combo SUCCESS (fw + fs, fresh 12:48, alpha.387+c459331, images under %LOCALAPPDATA%/OTGW-capture/img-alpha387; the block is combo-only, HAS_RUNTIME_HW_DETECT); evaluate.py --quick 70 passed / 0 / 0.

2026-09-30 correction: the reachability argument above ('auto mode closes the PIC UART, so this branch only runs from a forced Classic mode') holds for real PIC banners, but not for REPLAYED lines: since TASK-1071 the replay feeds processOT() on OT-Direct boards too, so a user log containing a boot banner can reach this branch on a combo OTGW32 in auto mode. That hazard predates this task (the old code persisted 0 -> 1 as well) and is tracked and fixed in TASK-1181.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
processOT()'s banner recovery on the combo board rewrote any board mode other than 1 to 1. That path only runs from a forced Classic mode (auto mode closes the PIC UART on a missed probe), so in practice it could only turn an S3 Mini Pro (mode 3) into a plain S3 Mini pin map on the next boot. It now learns the board only from auto mode (0), with the detected variant (bClassicPro ? 3 : 1). Proven on the host with test/host/test_banner_board_mode.py, which compiles the real block: the old code rewrites 3 to 1 and picks the wrong variant in auto mode; the fix keeps 3 and learns the right variant. Combo build and evaluate.py --quick are green at alpha.387.
<!-- SECTION:FINAL_SUMMARY:END -->
