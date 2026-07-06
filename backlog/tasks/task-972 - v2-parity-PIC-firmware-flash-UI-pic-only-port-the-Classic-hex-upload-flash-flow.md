---
id: TASK-972
title: >-
  v2 parity: PIC firmware flash UI (pic-only; port the Classic hex-upload/flash
  flow)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-01 05:21'
updated_date: '2026-07-06 20:42'
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

Authorized PIC flash attempted 2026-07-06 on the bench Classic-S3 (COM8, 192.168.88.64): selected gateway.hex (the currently-running v6.6, chosen deliberately as the safest possible real test -- a no-op re-flash if it succeeds). Drove the actual v2 UI via CDP (Advanced > PIC firmware, two-click arm/confirm on the real flash button, not a curl bypass). RESULT: failed both attempts with 'Error: Too many retries' (OTGWSerial.cpp stateMachine, OTGW_ERROR_RETRIES) at 0% progress -- this is the FWSTATE_RSET (bootloader-entry handshake) stage per the code (OTGWSerial.cpp:552, 10-try cap on non-CODE/DATA stages), meaning it failed to get the PIC into bootloader mode at all, well before any erase/write. serial->resetPic() runs on this failure path (OTGWSerial.cpp:554), safely resetting the PIC back to its normal running state.\n\nSAFETY CONFIRMED both times: live telnet 'a' (Send PR=A) command got a real response from the PIC ('Current firmware version: 6.6, device id: pic16f1847, firmware type: gateway') after each failed attempt -- the PIC is genuinely alive and unaffected, not just reporting stale cached data. /api/v2/device/info's picfwversion/picdeviceid/picfwtype also unchanged. /api/v2/firmware/files confirms gateway.hex/diagnose.hex/interface.hex are all still present on the device (a UI render race made the picFiles card briefly show 'No firmware files' after the failed attempt -- a frontend fetch-timing cosmetic bug, not data loss; confirmed via the REST endpoint directly).\n\nDid not attempt a third time. Two failed handshake attempts plus a genuinely safety-confirmed PIC state is enough evidence that this isn't a one-off fluke, and repeatedly hammering a bootloader-entry sequence on real hardware isn't something to do speculatively even though each attempt so far has proven safe. Screenshots in docs/evidence/ (task972_01 through 05). Root cause of the handshake failure not diagnosed further this pass -- possible causes worth investigating with physical access: serial timing/baud mismatch specific to this S3 Mini's PIC bridge wiring, a concurrent serial access conflict (though OTGW-Sim was confirmed OFF), or a genuine hardware/wiring marginality on this particular bench unit.\n\nAC#2 remains open. Recommend either: (a) investigate the handshake failure with a USB serial monitor for better diagnostics than the REST error string gives, or (b) try on a different PIC-equipped board to rule out this specific unit's wiring.

2026-07-06 investigation (dev, esp32-classic bench, .64): compared otgw-1.x.x vs dev for the PIC-flash code path.

OTGWSerial.cpp/.h: only ONE platform conditional exists in the whole file (the
constructor's UART/pin binding: ESP8266 uses fixed UART0, ESP32 uses UART1 with
explicit rxPin/txPin). Confirmed boards.h passes PIN_PIC_RX=44/PIN_PIC_TX=43
explicitly (matches ADR-158 combo pin scheme) -- not falling back to the
hardcoded 16/17 default. Retry counts, RSET handshake timing, and the whole
upgrade state machine are byte-identical between branches. Nothing to port at
the library level.

dev's architecture DOES add something 1.x never had: a dedicated FreeRTOS
PIC-UART task (TASK-865.6) that must "park" before the loop-side flash FSM can
drive the UART (waitForPICTaskParked(), picSerialPumpUpgrade()). Verified the
park handoff is correctly ordered (bPICactive=true is set BEFORE
waitForPICTaskParked() is called, OTGW-Core.ino:5522-5532). doBackgroundTasks()
also correctly isolates the flash-only path during a flash (skips
handleMQTT/handlePICSerial/OT-direct in the main loop while bPICactive).

Hypothesis: BLE scanning (separate FreeRTOS task, known core-1 starvation per
TASK-879) could still delay picSerialPumpUpgrade()'s polling cadence enough to
time out the PIC bootloader handshake, since HTTP/BLE run on tasks independent
of doBackgroundTasks()'s flash-only branch.

TESTED: disabled satbleenable, retried the flash live (user-authorized,
non-destructive attempt). SAME failure: Result code 9, "Too many retries",
10 retries, 0 errors -- byte-identical to the BLE-on attempts. This REFUTES
the BLE/core-1-starvation hypothesis as sole cause (re-enabled satbleenable
after the test).

Remaining candidates, unconfirmed: (a) genuine ESP32 UART1/GPIO-matrix timing
incompatibility with the PIC bootloader's expected handshake response window
(silicon-level, would need OTGWSerial timing-constant tuning to test), (b) a
hardware/connection issue specific to this bench unit -- but note the SAME
symptom occurred on this board both BEFORE and AFTER an unrelated board swap
(OTGW32 was connected in between), so a disturbed-connection theory from that
specific swap doesn't fit either; if it's hardware, it predates today's swap.

3 real flash attempts against this board today, all "Too many retries" at the
handshake stage (0 write errors -- never reached actual programming). Stopping
further live attempts pending either (a) a scope/logic-analyzer look at the
RSET handshake timing, or (b) trying against a different PIC-equipped board to
isolate silicon-timing vs this-specific-unit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC#1 done and verified. AC#2 (real PIC flash field validation): attempted twice with maintainer authorization, both attempts failed safely at the bootloader-entry handshake stage (before any erase/write) -- PIC confirmed alive and completely unchanged both times via a live serial command, not just cached data. Not a data-destructive failure, but the actual flash mechanism has not yet been proven to work end-to-end on this hardware. Left In Progress; root cause of the handshake failure needs further investigation, ideally with physical/USB serial access for better diagnostics than the REST API surfaces.
<!-- SECTION:FINAL_SUMMARY:END -->
