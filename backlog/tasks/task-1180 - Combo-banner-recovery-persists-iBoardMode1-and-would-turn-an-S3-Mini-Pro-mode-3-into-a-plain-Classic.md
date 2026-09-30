---
id: TASK-1180
title: >-
  Combo banner recovery persists iBoardMode=1 and would turn an S3 Mini Pro
  (mode 3) into a plain Classic
status: To Do
assignee: []
created_date: '2026-09-30 10:32'
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
- [ ] #1 The banner recovery never writes an iBoardMode that differs from the board's detected variant: either the persist is removed (it cannot trigger from auto mode) or it writes state.hw.bClassicPro ? 3 : 1 only when iBoardMode is 0
- [ ] #2 Old-vs-fix proof: a host harness that compiles the real banner branch (or a Classic S3 Mini Pro on the bench with iBoardMode 3 and a forced boot-probe miss) shows OLD rewriting 3 to 1 and FIX keeping 3
- [ ] #3 The change ships in one commit with its own prerelease bump; build.bat esp32-combo SUCCESS; evaluate.py no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Filed 2026-09-30 during TASK-1179. Reachability argued from OTGW-firmware.ino setup(): auto-mode detection failure closes the PIC UART and starts OT-Direct.
<!-- SECTION:NOTES:END -->
