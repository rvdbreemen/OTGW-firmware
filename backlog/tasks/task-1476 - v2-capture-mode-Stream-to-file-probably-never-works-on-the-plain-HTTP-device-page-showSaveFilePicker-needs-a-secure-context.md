---
id: TASK-1476
title: >-
  v2 capture mode: Stream-to-file probably never works on the plain-HTTP device
  page (showSaveFilePicker needs a secure context)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T14:07:00Z'
updated_date: '2026-10-29T23:59:00Z'
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
- [ ] #2 If confirmed: Stream-to-file replaced by a mechanism that works over HTTP (e.g. the periodic auto-download already present), tooltip/text corrected; screenshot before/after
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 bench evidence 2026-10-29 (session otgw-firmware-d5, web UI only, no telnet/flash/reboot): Playwright + installed Chrome (HeadlessChrome/156) on http://192.168.88.61/ Monitor > Log: isSecureContext=false, typeof window.showSaveFilePicker='undefined', chip title 'Stream to file needs Chrome / Edge', opacity 0.5, click leaves class 'tchip' (nothing happens). Control, same Chrome on http://127.0.0.1:8765/ (secure context): [true, 'function']. So the button is dead in Chrome itself on the device page, and the tooltip blames the browser. Screenshot: scratchpad t1476_before.png (toolbar crop only).

AC#2 implemented in worktree ../wt-1476 (base origin/dev c6caf2096), uncommitted, handed to coordinator otgw-firmware-da for the next batch (no build/bump/push here). Change: without showSaveFilePicker (plain HTTP device page, also Firefox/Safari) Stream to file collects every new frame (also while the display is paused) and downloads it as otgw-stream-<ts>.txt every 5 min, early at 200,000 lines, and the rest when switched off; with a secure context (HTTPS proxy) the file-picker path is unchanged. Chip no longer dimmed; tooltip says what it does. No API calls added (2-in-flight rule untouched). Evidence: node --check OK; evaluate.py --quick 79 pass / 0 fail; Playwright harness (scratchpad t1476_fallback.py: v2 data served over http on LAN IP 192.168.88.32 = insecure context, /ws faked with synthetic frames, clock mocked): OLD = chip title 'needs Chrome / Edge', opacity 0.5, click does nothing, 0 downloads; FIX = chip on, 0 downloads before 5 min, part 1 with frames 1-4 at 5 min (frame 4 sent while paused), part 2 with frames 5-6 on switch-off, no duplicates, no further downloads after off. NOT verified: the real file-picker path in a secure context (unchanged code), Chrome's 'allow multiple downloads' prompt on the 2nd part (same as existing Auto-download), and the on-device after-screenshot (after the batch flash).

Coordinator is now otgw-firmware-75 (replaces da). Local commit 07ed281a4 in ../wt-1476 (no bump/push/build), handed to 75. Host test tests/webui/log-stream-http-fallback.test.mjs: red on old v2.js (flushStreamChunk not found), 4/4 green on fix. Full webui suite 185/3: sat-page-order fails on base too; sat-status-pill + v2-navigation fail only in the full run (Chrome DevTools endpoint never came up), pass alone. evaluate --quick 79/0/0. Waiting: after-screenshot on device after batch flash.

2026-10-30 close-out of the batch: AC#2 code merged on origin/dev as 1e64da0d4 (alpha.539). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Host evidence (peer note above): tests/webui/log-stream-http-fallback.test.mjs red on the old v2.js, 4/4 green on the fix; Playwright insecure-context harness OLD 0 downloads / FIX parts at 5 min and on switch-off. AC#2 stays open: the on-device after-screenshot on http://<device>/ with alpha.539 or later is still to be taken.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Stream to file works on the plain-HTTP device page: without showSaveFilePicker (insecure context, also Firefox/Safari) the chip collects every new frame and downloads otgw-stream-<ts>.txt every 5 min, early at 200,000 lines, and the rest on switch-off; the secure-context file-picker path is unchanged; the chip is no longer dimmed and its tooltip says what it does. Merged 1e64da0d4, alpha.539.

Evidence per AC:
- AC#1: bench check 2026-10-29 in Chrome on http://192.168.88.61/: isSecureContext=false, typeof showSaveFilePicker='undefined', chip dimmed with 'needs Chrome / Edge' tooltip; control on 127.0.0.1 gives [true, 'function'].
- AC#2 (open): tests/webui/log-stream-http-fallback.test.mjs red on the old v2.js, 4/4 green on the fix; Playwright insecure-context harness OLD 0 downloads, FIX part 1 at 5 min and part 2 on switch-off; before screenshot t1476_before.png. Missing: the on-device after-screenshot.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
