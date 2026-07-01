---
id: TASK-972
title: >-
  v2 parity: PIC firmware flash UI (pic-only; port the Classic hex-upload/flash
  flow)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 05:21'
updated_date: '2026-07-01 19:34'
labels: []
dependencies: []
ordinal: 184000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Last Classic->v2 gap (from the parity analysis + TASK-968 note). Classic Advanced > 'PIC firmware' (tabPICflash, pic-only) flashes the OTGW PIC over serial: lists available .hex files, /api/v2/pic/update-check, /pic?action=refresh (fetch hex), /pic?action=upgrade&name=X (flash), progress/status, /api/v2/pic/settings. v2 has none of it. CONSTRAINTS: (1) large flow (hundreds of lines of classic JS); (2) SAFETY-CRITICAL — a wrong/interrupted PIC flash bricks the PIC; (3) UNVERIFIABLE on the OTGW32 bench (.39 is OT-Direct, no PIC -> the feature is gated off), so on-hardware verification needs a real PIC board (field validation). Options: (A) full port into a v2 Advanced/PIC section, gated pic-only, field-tested on PIC hardware; (B) a v2 pic-only section that shows the PIC version and bridges to the Classic UI for the actual flash. Endpoints already exist (handlePic/upgradepic).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 v2 exposes a pic-only PIC firmware entry (hidden on OTGW32/OT-Direct); on a PIC board it lists firmware + flashes via the existing /pic endpoints with progress/status
- [ ] #2 Verified: hidden on .39 (no PIC); flash flow field-validated on a real PIC board
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented the full PIC-flash port into v2 (data-only, v2.html + v2.js). New 'PIC firmware' card in the Debug tab (Monitor > Debug), gated on otcommandinterface==='PIC' (hidden on OTGW32/OT-Direct). Reuses the existing firmware endpoints: GET /api/v2/device/info (picdeviceid/picfwtype/picfwversion), GET /api/v2/firmware/files (bare array of {name,version,size}), GET /pic?action=upgrade|refresh|delete (top-level route, PICBASE), GET /api/v2/pic/update-check (banner), GET /api/v2/pic/flash-status (progress). Per-file Flash (two-click confirm, brick-safety) / Re-download / Delete; Check-for-updates banner; a poll-based progress bar (polls flash-status every 1s — simpler + more robust than routing flash JSON through the v2 /ws handler). Reuses set-group/ble-row/ble-nm/ble-ctrls/ot-support-hint/tbtn (no design-system drift). Verified on .39 (OT-Direct, no PIC): card HIDDEN (correct gating), structure renders when forced (info+check+files+progress), Check-for-updates handled the no-PIC 503 gracefully ('Update check: unavailable'), 0 console errors. buildfs green; evaluate 98.7%/Failed 0. AC#2 (live flash flow: list/flash/progress on a PIC board) is FIELD VALIDATION — .39 has no PIC so it cannot exercise the actual flash; needs a real PIC board (e.g. the classic-S3 bench board once WiFi-provisioned).

Code complete and committed (22effb9df). AC#1 verified on .39 (card hidden on no-PIC OT-Direct, structure renders when forced, 503 handled, buildfs green, evaluate 0 failures). AC#2 flash-flow field-validation is HARDWARE-GATED: needs a real PIC board (classic-S3 bench, COM8, not yet WiFi-provisioned). SAFETY: two-click confirm guards the flash (wrong/interrupted PIC flash bricks the PIC). Held In Progress pending PIC-hardware field validation.
<!-- SECTION:NOTES:END -->
