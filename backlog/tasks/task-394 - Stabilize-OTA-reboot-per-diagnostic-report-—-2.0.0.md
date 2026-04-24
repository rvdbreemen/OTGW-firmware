---
id: TASK-394
title: Stabilize OTA reboot per diagnostic report — 2.0.0
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-24 00:26'
updated_date: '2026-04-24 00:26'
labels:
  - reboot
  - ota
  - arduino-core-3.1.2
  - diagnostics
  - reliability
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Address the secondary code-level hypotheses from deep-research-report_arduino_core_3.1.2_reboot_issue_after_OTA.md that remain open after TASK-392/393 fixed the upgrade-order documentation. Unifies all reboot paths through doRestart(), adds boot-signature diagnostics, introduces a deferred-reboot mechanism so HTTP responses flush before reset, and instruments the OTA flow with heap snapshots. Hardware validation is user-executed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Phase 1: runNightlyRestartCheck() routes through doRestart() instead of direct ESP.restart(); ESP32 OTA-success path also routes through doRestart() for symmetry with ESP8266. No direct ESP.restart/ESP.reset/platformRestart calls remain outside doRestart()/requestDeferredReboot()/platformRestart internals.
- [ ] #2 Phase 2: logBootSignature() helper exists in helperStuff.ino, is called once at end of setup(), and emits a single PROGMEM-formatted DebugTf line with core/sdk/cpu/flashId/flashReal/flashMap/flashSpeed/sketch/freeSketch/md5/heap/maxBlk/frag/resetReason fields.
- [ ] #3 Phase 3: requestDeferredReboot() + performDeferredReboot() + g_rebootPending flag exist; loop() checks the flag after doBackgroundTasks() and fires performDeferredReboot() only when !isFlashing(); both ESP8266 and ESP32 OTA-success handlers use requestDeferredReboot() so HTTP 200 is delivered before the reboot begins.
- [ ] #4 Phase 4: OTA flow logs heap-signature snapshots at 4 points — pre-begin (both firmware+FS paths), post-end, post-remount (FS only), pre-reboot — each prefixed [OTA] so the sequence is greppable.
- [ ] #5 python build.py completes cleanly on both ESP32 and ESP8266 with no new warnings or errors after each phase; python evaluate.py --quick shows no new FAILs attributable to this work. Phase 5 hardware validation plan documented; user-executed; not counted toward this task's code-completion.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Phase 1 — Unify reboot paths (2 small edits, low risk):
  a. OTGW-firmware.ino:434-435: runNightlyRestartCheck() -> doRestart("[nightly] scheduled restart")
  b. OTGW-ModUpdateServer-esp32.h:93-101: replace manual delay+platformRestart with doRestart("[OTA] Rebooting...")
  Verify: grep confirms no remaining direct ESP.restart/ESP.reset/platformRestart outside platformRestart internals.

Phase 2 — Boot signature logging (1 helper + 1 callsite):
  a. helperStuff.ino: add logBootSignature() above doRestart(); uses existing platformCoreVersion/SdkVersion/FlashChipId/RealSize/Size/Speed + ESP.getSketchMD5/Size/FreeSketchSpace + platformFreeHeap/MaxFreeBlock/ResetReason; emits one PSTR-formatted DebugTf line.
  b. OTGW-firmware.ino:192: call logBootSignature() after existing reset-reason log block so init state is captured.

Phase 3 — Deferred reboot mechanism (structural):
  a. helperStuff.ino: add static volatile bool g_rebootPending; static bool g_rebootHard; static const char *g_rebootReason; plus requestDeferredReboot(reason, hard=false) and [[noreturn]] performDeferredReboot() helpers.
  b. Expose prototypes via existing helperStuff.h pattern or forward-decl.
  c. OTGW-firmware.ino loop(): after doBackgroundTasks(), if g_rebootPending && !isFlashing() -> performDeferredReboot().
  d. OTGW-ModUpdateServer-impl.h:116: swap doRestart() -> requestDeferredReboot("[OTA] Rebooting...").
  e. OTGW-ModUpdateServer-esp32.h: same swap on the ESP32 side (after Phase 1 already put doRestart there).

Phase 4 — OTA heap instrumentation (4 probe points):
  a. _beginFirmwareUpload and _beginFilesystemUpload: logBootSignature prefixed "[OTA] pre-begin".
  b. _handleUploadEnd after Update.end(true): prefix "[OTA] post-end".
  c. _handleUploadEnd after LittleFS.begin() remount (FS only): "[OTA] post-remount".
  d. HTTP POST handler before requestDeferredReboot: "[OTA] pre-reboot".

After each phase:
  - python build.py (dual-platform) — confirm clean.
  - python evaluate.py --quick — confirm no regression.
  - Commit as its own commit with conventional-commit prefix.

Phase 5 (hardware, user-executed) — not coded by me.
<!-- SECTION:PLAN:END -->
