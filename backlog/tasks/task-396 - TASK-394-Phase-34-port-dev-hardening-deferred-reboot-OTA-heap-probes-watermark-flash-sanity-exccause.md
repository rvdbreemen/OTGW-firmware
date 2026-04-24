---
id: TASK-396
title: >-
  TASK-394 Phase 3+4 port + dev hardening: deferred reboot, OTA heap probes,
  watermark, flash sanity, exccause
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-24 08:07'
updated_date: '2026-04-24 08:07'
labels:
  - reboot
  - ota
  - diagnostics
  - arduino-core-3.1.2
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the remaining diagnostic-report recommendations on dev (1.4.x): deferred reboot mechanism to get reboots out of HTTP callback context, OTA heap instrumentation at 4 lifecycle points, heap watermark tracking for slow-leak detection, flash-config sanity check at boot, exccause field in boot signature, plus rich debug logging around prepareForReboot() and doRestart() timing. Three commits in build-safe order: helpers first (dead code compiles), wiring second (helpers called from setup/loop), OTA integration third (HTTP handler swaps to deferred path).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 helperStuff.ino exposes requestDeferredReboot(reason), performDeferredReboot(), isRebootPending(), rebootHeapWatermarkTick(), getMinFreeHeap(), maybeWarnFlashMismatch(); logBootSignature() output includes minHeap and exccause fields; prepareForReboot() and doRestart() log per-step timing and heap snapshots
- [ ] #2 loop() calls rebootHeapWatermarkTick() and checks isRebootPending() && !isFlashing() after doBackgroundTasks(); setup() calls maybeWarnFlashMismatch() after the boot signature
- [ ] #3 OTGW-ModUpdateServer-impl.h emits logBootSignature("[OTA] pre-begin") / post-end / post-remount / pre-reboot at the 4 lifecycle points; HTTP POST success handler swaps doRestart() for requestDeferredReboot() so browser gets clean 200 before reboot fires
- [ ] #4 Three commits: (A) helpers only, (B) setup+loop wiring, (C) OTA integration. Each builds cleanly on its own with no bisect gap. python build.py --firmware passes on all three intermediate states.
- [ ] #5 No regressions: python evaluate.py --quick remains 0 fail; no direct ESP.restart/ESP.reset outside helperStuff's doRestart() and the pre-existing networkStuff.ino WiFi-portal exception
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read helperStuff.ino prepareForReboot+doRestart full bodies for exact edit context.
2. Read OTGW-firmware.ino loop() for doBackgroundTasks() insertion point.
3. Read OTGW-ModUpdateServer-impl.h around lines 108/174/197/297/300 for probe insertion points.
4. Commit A (helpers): extend helperStuff.ino with watermark + deferred-reboot + flash sanity helpers + timing instrumentation in prepareForReboot and doRestart; extend logBootSignature with minHeap + exccause; add prototypes in OTGW-firmware.h.
5. Build + evaluate. Commit A.
6. Commit B (wiring): setup() calls maybeWarnFlashMismatch; loop() updates watermark + checks isRebootPending after doBackgroundTasks.
7. Build + evaluate. Commit B.
8. Commit C (OTA): 4 logBootSignature probes in impl.h; swap doRestart for requestDeferredReboot in HTTP success handler.
9. Build + evaluate. Commit C.
10. Build-bump commit.
11. Push origin/dev.
<!-- SECTION:PLAN:END -->
