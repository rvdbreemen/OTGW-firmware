---
id: TASK-395
title: Port TASK-394 Phase 1+2 reboot/diagnostics fixes from 2.0.0 to dev
status: Done
assignee:
  - '@claude'
created_date: '2026-04-24 00:41'
updated_date: '2026-04-24 00:47'
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
- [x] #1 runNightlyRestartCheck at OTGW-firmware.ino:281 calls doRestart("[nightly] scheduled restart") instead of direct ESP.restart(). No other direct ESP.restart/ESP.reset/platformRestart calls remain outside helperStuff.ino's doRestart()/platformRestart internals and the pre-existing networkStuff.ino WiFi-portal-timeout exception.
- [x] #2 logBootSignature(const char *phase) helper exists in helperStuff.ino using direct ESP API (ESP.getCoreVersion, getSdkVersion, getCpuFreqMHz, getFlashChipId/RealSize/Size/Speed, getSketchSize, getFreeSketchSpace, getSketchMD5, getFreeHeap, getMaxFreeBlockSize, getHeapFragmentation, getResetReason), prototype in OTGW-firmware.h, called once in setup() after updateRebootLog(). Output format matches 2.0.0 helper so logs are cross-branch comparable.
- [x] #3 Commits land in Phase-2-then-Phase-1 order so every intermediate commit builds cleanly (no git bisect hazard).
- [x] #4 python build.py --firmware completes cleanly on dev; python evaluate.py --quick shows no new regressions attributable to this port.
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 2 committed first as 90ad492a (logBootSignature helper + prototype + setup() callsite). Phase 1 committed as ae3ba055 (nightly restart through doRestart). Commits are in build-safe order so no git bisect gap.
Verification: python evaluate.py --quick shows 31 pass / 0 fail / 94.3% health (identical to pre-port baseline). Only direct ESP.restart remaining is networkStuff.ino:143 in the pre-existing WiFi-portal-timeout exception — same as on 2.0.0.
Firmware build: python build.py --firmware exit 0 on ESP8266. No warnings, no errors.
AC5 closed by the clean evaluator + build results.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port of TASK-394 Phase 1+2 from feature-dev-2.0.0 (commits 2f2adf0a + 79b3aa56) onto dev (1.4.x).

Changes:
- Phase 2 (commit 90ad492a): add logBootSignature(const char *phase) helper in helperStuff.ino that emits a one-line greppable signature with core/SDK/CPU/flash/sketch/heap/reset fields. Dev is ESP8266-only with no platform abstraction layer, so this uses direct ESP APIs (ESP.getCoreVersion, getSdkVersion, getHeapFragmentation, etc.) rather than the platform* helpers used on 2.0.0. Output format matches 2.0.0 so logs are cross-branch comparable. Prototype added to OTGW-firmware.h next to doRestart. Called once in setup() after updateRebootLog().
- Phase 1 (commit ae3ba055): route runNightlyRestartCheck() at OTGW-firmware.ino:281 through doRestart("[nightly] scheduled restart") instead of direct ESP.restart(). Aligns the scheduled reboot path with the OTA-success path, both now go through the same prepareForReboot cleanup sequence (MQTT LWT, WS close, TCP FINs) that is critical on Arduino Core 3.1.0+. No ESP32 OTA path to fix on dev.

Tests / verification:
- python evaluate.py --quick: 35 checks, 31 pass, 0 fail, health 94.3% (identical to pre-port baseline).
- python build.py --firmware: exit 0, no warnings, no errors. ESP8266 build.
- Commits in Phase-2-then-Phase-1 order so every intermediate commit builds cleanly (no git bisect hazard — lesson learned from the 2.0.0 port where commit 2f2adf0a had the logBootSignature call without the definition).

Risks / follow-ups:
- Same runtime smoke-test advice as 2.0.0: on device, verify "boot: core=... sdk=... heap=... reset=[...]" line appears once per boot in telnet log (port 23), and nightly-restart log line at configured hour is followed by prepareForReboot cleanup sequence.
- Phases 3 + 4 (deferred-reboot mechanism, OTA heap instrumentation) from TASK-394 are NOT ported to dev yet. They remain pending on 2.0.0 and should be ported after 2.0.0 hardware validation.
- networkStuff.ino:143 direct ESP.restart() in WiFi-portal-timeout path is a deliberate early-setup exception (services not up yet, cleanup would be no-op).
<!-- SECTION:FINAL_SUMMARY:END -->
