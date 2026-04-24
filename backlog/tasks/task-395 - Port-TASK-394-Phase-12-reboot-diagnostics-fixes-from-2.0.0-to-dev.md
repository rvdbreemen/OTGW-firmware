---
id: TASK-395
title: Port TASK-394 Phase 1+2 reboot/diagnostics fixes from 2.0.0 to dev
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-24 00:41'
updated_date: '2026-04-24 00:41'
labels:
  - port
  - reboot
  - diagnostics
  - arduino-core-3.1.2
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the two commits already on feature-dev-2.0.0 (2f2adf0a reboot fix + 79b3aa56 logBootSignature diagnostics) onto dev (1.4.x). Dev has the same doRestart/prepareForReboot/runNightlyRestartCheck scaffolding so the structural port is clean; logBootSignature needs to be rewritten with direct ESP API calls instead of platform* helpers because dev is ESP8266-only without a platform abstraction layer. No ESP32 OTA path on dev, so Phase 1 is a single edit here.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 runNightlyRestartCheck at OTGW-firmware.ino:281 calls doRestart("[nightly] scheduled restart") instead of direct ESP.restart(). No other direct ESP.restart/ESP.reset/platformRestart calls remain outside helperStuff.ino's doRestart()/platformRestart internals and the pre-existing networkStuff.ino WiFi-portal-timeout exception.
- [ ] #2 logBootSignature(const char *phase) helper exists in helperStuff.ino using direct ESP API (ESP.getCoreVersion, getSdkVersion, getCpuFreqMHz, getFlashChipId/RealSize/Size/Speed, getSketchSize, getFreeSketchSpace, getSketchMD5, getFreeHeap, getMaxFreeBlockSize, getHeapFragmentation, getResetReason), prototype in OTGW-firmware.h, called once in setup() after updateRebootLog(). Output format matches 2.0.0 helper so logs are cross-branch comparable.
- [ ] #3 Commits land in Phase-2-then-Phase-1 order so every intermediate commit builds cleanly (no git bisect hazard).
- [ ] #4 python build.py --firmware completes cleanly on dev; python evaluate.py --quick shows no new regressions attributable to this port.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Phase 2 first (avoids bisect gap):
  a. helperStuff.ino: add void logBootSignature(const char *phase) above doRestart() using direct ESP API (ESP.getHeapFragmentation works native on ESP8266, no need to compute).
  b. OTGW-firmware.h: add prototype next to doRestart declaration (~line 137).
  c. OTGW-firmware.ino: call logBootSignature("boot:") after updateRebootLog(lastReset) and before "Setup finished!" log.
  d. Commit: feat(diagnostics): add logBootSignature() for boot telemetry (dev).

Phase 1 second:
  e. OTGW-firmware.ino:281: replace delay(200); ESP.restart(); with doRestart("[nightly] scheduled restart").
  f. Commit: fix(reboot): route nightly restart through doRestart() (dev).

Verify after each commit: grep confirms no direct ESP.restart/ESP.reset outside helperStuff.ino and the networkStuff WiFi-portal exception.

After both commits: python build.py --firmware (dev is ESP8266-only, one platform), python evaluate.py --quick, then push.
<!-- SECTION:PLAN:END -->
