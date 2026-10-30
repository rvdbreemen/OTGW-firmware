---
id: TASK-1476
title: >-
  v2 capture mode: Stream-to-file probably never works on the plain-HTTP device
  page (showSaveFilePicker needs a secure context)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-29T14:07:00Z'
updated_date: '2026-10-30T00:10:00Z'
labels:
  - webui
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 636000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in TASK-1472 research: v2.js:3541/5752 feature-detects window.showSaveFilePicker; the File System Access API is exposed only in secure contexts, and the device serves plain HTTP (no HTTPS by project rule), so the button is likely always disabled with a misleading 'needs Chrome / Edge' tooltip. Not verified on a bench yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench check on http://<device>/ in Chrome: isSecureContext and typeof showSaveFilePicker recorded
- [x] #2 If confirmed: Stream-to-file replaced by a mechanism that works over HTTP (e.g. the periodic auto-download already present), tooltip/text corrected; screenshot before/after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 bench evidence 2026-10-29 (session otgw-firmware-d5, web UI only, no telnet/flash/reboot): Playwright + installed Chrome (HeadlessChrome/156) on http://192.168.88.61/ Monitor > Log: isSecureContext=false, typeof window.showSaveFilePicker='undefined', chip title 'Stream to file needs Chrome / Edge', opacity 0.5, click leaves class 'tchip' (nothing happens). Control, same Chrome on http://127.0.0.1:8765/ (secure context): [true, 'function']. So the button is dead in Chrome itself on the device page, and the tooltip blames the browser. Screenshot: scratchpad t1476_before.png (toolbar crop only).

AC#2 implemented in worktree ../wt-1476 (base origin/dev c6caf2096), uncommitted, handed to coordinator otgw-firmware-da for the next batch (no build/bump/push here). Change: without showSaveFilePicker (plain HTTP device page, also Firefox/Safari) Stream to file collects every new frame (also while the display is paused) and downloads it as otgw-stream-<ts>.txt every 5 min, early at 200,000 lines, and the rest when switched off; with a secure context (HTTPS proxy) the file-picker path is unchanged. Chip no longer dimmed; tooltip says what it does. No API calls added (2-in-flight rule untouched). Evidence: node --check OK; evaluate.py --quick 79 pass / 0 fail; Playwright harness (scratchpad t1476_fallback.py: v2 data served over http on LAN IP 192.168.88.32 = insecure context, /ws faked with synthetic frames, clock mocked): OLD = chip title 'needs Chrome / Edge', opacity 0.5, click does nothing, 0 downloads; FIX = chip on, 0 downloads before 5 min, part 1 with frames 1-4 at 5 min (frame 4 sent while paused), part 2 with frames 5-6 on switch-off, no duplicates, no further downloads after off. NOT verified: the real file-picker path in a secure context (unchanged code), Chrome's 'allow multiple downloads' prompt on the 2nd part (same as existing Auto-download), and the on-device after-screenshot (after the batch flash).

Coordinator is now otgw-firmware-75 (replaces da). Local commit 07ed281a4 in ../wt-1476 (no bump/push/build), handed to 75. Host test tests/webui/log-stream-http-fallback.test.mjs: red on old v2.js (flushStreamChunk not found), 4/4 green on fix. Full webui suite 185/3: sat-page-order fails on base too; sat-status-pill + v2-navigation fail only in the full run (Chrome DevTools endpoint never came up), pass alone. evaluate --quick 79/0/0. Waiting: after-screenshot on device after batch flash.

2026-10-30 close-out of the batch: AC#2 code merged on origin/dev as 1e64da0d4 (alpha.539). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Host evidence (peer note above): tests/webui/log-stream-http-fallback.test.mjs red on the old v2.js, 4/4 green on the fix; Playwright insecure-context harness OLD 0 downloads / FIX parts at 5 min and on switch-off. AC#2 stays open: the on-device after-screenshot on http://<device>/ with alpha.539 or later is still to be taken.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

AC#2 on-device after (2026-10-30 ~00:08Z, .63 Classic+S3, alpha.542+c00c5f0 incl. 07ed281a4): Playwright + Chrome 156 on http://192.168.88.63/ Monitor > Log: isSecureContext=false, showSaveFilePicker undefined, chip not dimmed (opacity ''), title 'Save every frame: downloads the new frames as a file every 5 minutes and when you switch it off', click -> class 'tchip on'. Screenshot scratchpad t1476_after.png (toolbar crop, no device data).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stream to file in Monitor > Log never worked on the device page: showSaveFilePicker exists only in a secure context and the device serves plain HTTP, so the chip was dimmed, a no-op, and its tooltip blamed the browser. Without a picker the chip now collects every new frame (also while the display is paused) and downloads otgw-stream-<ts>.txt every 5 minutes, early at LOG_CAPTURE lines, and the remainder on switch-off; each part holds only the frames since the previous part. The file-picker path in a secure context (HTTPS proxy) is unchanged; no API calls added. Evidence: AC#1 bench .61 (Chrome on http://192.168.88.61/: isSecureContext=false, picker undefined, chip dimmed 'needs Chrome / Edge', click no-op; same Chrome on 127.0.0.1: [true,'function']; t1476_before.png). AC#2 host test tests/webui/log-stream-http-fallback.test.mjs runs the shipped pushLog/flushStreamChunk/toggleStream: red on the old v2.js (flushStreamChunk not found), 4/4 green; Playwright old-vs-fix over HTTP on a LAN IP with synthetic /ws frames (old 0 downloads; fix part 1 frames 1-4 incl. one sent while paused, part 2 frames 5-6 on switch-off); on-device after on .63 alpha.542 (chip not dimmed, new title, click -> on; t1476_after.png). On dev as 1e64da0d4 (local commit 07ed281a4, rebased into the batch), shipped in alpha.542. Not verified: the picker path in a secure context (unchanged code) and Chrome's 'allow multiple downloads' prompt on the second part (same as Auto-download).
<!-- SECTION:FINAL_SUMMARY:END -->
