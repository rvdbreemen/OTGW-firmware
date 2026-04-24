---
id: TASK-396
title: >-
  TASK-394 Phase 3+4 port + dev hardening: deferred reboot, OTA heap probes,
  watermark, flash sanity, exccause
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 08:07'
updated_date: '2026-04-24 08:29'
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
- [x] #1 helperStuff.ino exposes requestDeferredReboot(reason), performDeferredReboot(), isRebootPending(), rebootHeapWatermarkTick(), getMinFreeHeap(), maybeWarnFlashMismatch(); logBootSignature() output includes minHeap and exccause fields; prepareForReboot() and doRestart() log per-step timing and heap snapshots
- [x] #2 loop() calls rebootHeapWatermarkTick() and checks isRebootPending() && !isFlashing() after doBackgroundTasks(); setup() calls maybeWarnFlashMismatch() after the boot signature
- [x] #3 OTGW-ModUpdateServer-impl.h emits logBootSignature("[OTA] pre-begin") / post-end / post-remount / pre-reboot at the 4 lifecycle points; HTTP POST success handler swaps doRestart() for requestDeferredReboot() so browser gets clean 200 before reboot fires
- [x] #4 Three commits: (A) helpers only, (B) setup+loop wiring, (C) OTA integration. Each builds cleanly on its own with no bisect gap. python build.py --firmware passes on all three intermediate states.
- [x] #5 No regressions: python evaluate.py --quick remains 0 fail; no direct ESP.restart/ESP.reset outside helperStuff's doRestart() and the pre-existing networkStuff.ino WiFi-portal exception
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Commit A 696bdb91 (helpers + instrumentation): added deferred-reboot, heap watermark, flash sanity helpers + [reboot] timing logs in prepareForReboot and doRestart + minHeap/exccause in logBootSignature. Dead-code compile verified clean.
Commit B ab443d06 (wiring): setup() calls maybeWarnFlashMismatch, loop() updates watermark and checks isRebootPending+!isFlashing after doBackgroundTasks.
Commit C 378538d8 (OTA integration): 4 logBootSignature probes ([OTA] pre-begin/post-end/post-remount/pre-reboot) + HTTP success handler swapped doRestart for requestDeferredReboot.
All three builds clean (no warnings, no errors). Evaluator 31 pass / 0 fail / 94.3% health — identical to pre-port baseline.
Build-bump 1.4.2-beta+378538d (3169).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Implements the remaining diagnostic-report recommendations on dev in one atomic set of three build-safe commits, with rich telnet instrumentation around every reboot path.

Changes:
- Commit A (696bdb91, helpers only, dead-code-compiles): new helpers in helperStuff.ino — rebootHeapWatermarkTick/getMinFreeHeap (slow-leak detection), requestDeferredReboot/performDeferredReboot/isRebootPending (deferred-reboot mechanism), maybeWarnFlashMismatch (flash config sanity at boot). prepareForReboot() and doRestart() instrumented with per-step timing and heap snapshots so the full reboot choreography is visible in telnet. logBootSignature() extended with minHeap + exccause fields. Prototypes in OTGW-firmware.h.
- Commit B (ab443d06, wiring): setup() calls maybeWarnFlashMismatch() after the boot signature; loop() updates the heap watermark every tick and checks the deferred-reboot gate after doBackgroundTasks().
- Commit C (378538d8, OTA integration): four logBootSignature probes — [OTA] pre-begin (both firmware and FS variants) / post-end / post-remount (FS only) / pre-reboot — and the HTTP success handler now calls requestDeferredReboot() instead of doRestart() so HTTP 200 responses flush to the browser before service cleanup starts.

Tests / verification:
- python build.py --firmware passed on all three intermediate states. No warnings, no errors.
- python evaluate.py --quick: 35 total, 31 pass, 0 fail, health 94.3% — identical to pre-TASK-396 baseline. No new gate regressions.
- Direct ESP.restart/ESP.reset grep: only remaining calls are inside helperStuff.ino doRestart() (authorized primitive) and the pre-existing networkStuff.ino WiFi-portal-timeout early-setup exception.

Rich debug output around reboot:
- Boot: "boot: core=... sdk=... flashId=... heap=... minHeap=... exccause=... reset=[...]"
- Flash mismatch: "[flash] WARN: real size X != mapped size Y ..." (only if mismatch detected)
- Reboot request (deferred): "[reboot] deferred request: \"<reason>\" heap=... minHeap=... maxBlk=... frag=... flashing=..."
- Reboot fire: "[reboot] performing deferred reboot after Xms defer: \"<reason>\""
- Pre-doRestart snapshot: "[reboot] pre-doRestart core=... heap=... reset=[...]"
- doRestart phase: "[reboot] doRestart(\"...\") begin, heap=... minHeap=... maxBlk=... frag=..."
- Settings flush: "[reboot]   flushSettings: Xms"
- Cleanup: "[reboot] prepareForReboot begin, heap=... maxBlk=..."
                 "[reboot]   mqtt disconnect: Xms"
                 "[reboot]   ws close: Xms"
                 "[reboot]   stopping telnet+otgwstream, total=Xms heap=..."
- Final (serial only, after telnet dies): "[reboot]   calling ESP.reset() after Xms total"
- OTA lifecycle: "[OTA] pre-begin ... post-end ... post-remount ... pre-reboot" each with full boot-signature-style fields.

Risks / follow-ups:
- Runtime validation on real hardware per the test plan in conversation 2026-04-24 is the next gate. The 20x OTA stress test is the primary acceptance criterion (no WDT, no exception, no FS mount failure, minHeap > 4000 throughout).
- Port to 2.0.0: Phase 3 + Phase 4 equivalents pending under TASK-394. Should be done after dev validation so we know the design works before replicating.
- Pre-existing cosmetic: commit 2f2adf0a on 2.0.0 has a logBootSignature call without its definition (build-break only for someone bisecting exactly on that commit). Reparable by revert+redo if bisect hygiene becomes important.
<!-- SECTION:FINAL_SUMMARY:END -->
