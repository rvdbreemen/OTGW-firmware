---
id: TASK-394
title: Stabilize OTA reboot per diagnostic report — 2.0.0
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 00:26'
updated_date: '2026-04-24 18:59'
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
- [x] #1 Phase 1: runNightlyRestartCheck() routes through doRestart() instead of direct ESP.restart(); ESP32 OTA-success path also routes through doRestart() for symmetry with ESP8266. No direct ESP.restart/ESP.reset/platformRestart calls remain outside doRestart()/requestDeferredReboot()/platformRestart internals.
- [x] #2 Phase 2: logBootSignature() helper exists in helperStuff.ino, is called once at end of setup(), and emits a single PROGMEM-formatted DebugTf line with core/sdk/cpu/flashId/flashReal/flashMap/flashSpeed/sketch/freeSketch/md5/heap/maxBlk/frag/resetReason fields.
- [x] #3 Phase 3: requestDeferredReboot() + performDeferredReboot() + g_rebootPending flag exist; loop() checks the flag after doBackgroundTasks() and fires performDeferredReboot() only when !isFlashing(); both ESP8266 and ESP32 OTA-success handlers use requestDeferredReboot() so HTTP 200 is delivered before the reboot begins.
- [x] #4 Phase 4: OTA flow logs heap-signature snapshots at 4 points — pre-begin (both firmware+FS paths), post-end, post-remount (FS only), pre-reboot — each prefixed [OTA] so the sequence is greppable.
- [x] #5 python build.py completes cleanly on both ESP32 and ESP8266 with no new warnings or errors after each phase; python evaluate.py --quick shows no new FAILs attributable to this work. Phase 5 hardware validation plan documented; user-executed; not counted toward this task's code-completion.
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

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fully resolved via the dev→2.0.0 merge (commit e1f8e070) plus the post-merge completion work (TASK-406 ESP32 OTA parity, TASK-404/405/407 platform-abstraction hygiene).

**Phase-by-phase status on the merged branch:**

- **Phase 1 — Unified reboot paths (AC #1):** `runNightlyRestartCheck()` routes through `doRestart()` on both platforms. ESP32 OTA success handler previously called `doRestart()` directly from inside the HTTP callback; TASK-406 converted that to `requestDeferredReboot()` for parity with ESP8266. H2 investigation confirmed: the only non-doRestart restart paths are the documented WiFi-portal-timeout exception in networkStuff.ino and the `platformRestart()` definitions inside platform_*.h themselves.

- **Phase 2 — Boot signature (AC #2):** `logBootSignature(const char *phase)` exists in helperStuff.ino, called from `setup()` after `updateRebootLog()` with phase="boot:". Output format includes all specified fields plus `minHeap` and `exccause` (dual-target via the platform abstractions added in commit 38f57c5a).

- **Phase 3 — Deferred reboot mechanism (AC #3):** `requestDeferredReboot()` / `performDeferredReboot()` / `isRebootPending()` / `g_rebootPending` all live in helperStuff.ino. Loop gate at `OTGW-firmware.ino:648`: `if (isRebootPending() && !isFlashing()) performDeferredReboot();`. Both ESP8266 (OTGW-ModUpdateServer-impl.h:121) and ESP32 (OTGW-ModUpdateServer-esp32.h, via TASK-406) OTA success paths use `requestDeferredReboot()`.

- **Phase 4 — OTA heap instrumentation (AC #4):** Four probes on ESP8266 (pre-begin × 2 for firmware+FS, post-end, post-remount, pre-reboot). Four matching probes on ESP32 added via TASK-406 at the corresponding lifecycle points.

- **AC #5 builds clean:** Both `pio run -e esp8266` (Core 2.7.4) and `pio run -e esp32` (pioarduino 55.03.35) SUCCESS. No new evaluate.py FAILs attributable to this work.

**Phase 5 hardware validation** is explicitly out of scope per the original AC statement; user-executed on the test-device queue.

**Relevant commits:** e1f8e070 (merge), 38f57c5a (platform layer), b704243d (forward-decls), c90c94a5 (ESP32 parity).
<!-- SECTION:FINAL_SUMMARY:END -->
