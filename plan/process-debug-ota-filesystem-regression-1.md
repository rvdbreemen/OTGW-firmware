---
goal: Debug ESP8266 OTA LittleFS Regression Between v1.0.0-v1.2.0 and Current dev
version: 1.0
date_created: 2026-03-13
last_updated: 2026-03-13
owner: GitHub Copilot
status: Planned
tags: [process, debug, ota, filesystem, littlefs, esp8266, regression]
---

# Introduction

![Status: Planned](https://img.shields.io/badge/status-Planned-blue)

This plan defines a deterministic debug workflow to isolate why OTA flashing of the LittleFS image on ESP8266 is flaky on the current `dev` branch while earlier releases reportedly worked. The plan focuses on the exact OTA write path, the post-flash filesystem recovery path, the reboot/startup path, and release-to-release behavior changes across `v1.0.0`, `v1.1.0`, `v1.2.0`, and current `dev`.

## 1. Requirements & Constraints

- **REQ-001**: Determine whether the failure occurs during OTA upload, flash write, post-flash filesystem restore, reboot, or first boot after reboot.
- **REQ-002**: Compare the behavior of `v1.0.0`, `v1.1.0`, `v1.2.0`, and current `dev` using the same hardware and flashing scenario.
- **REQ-003**: Produce evidence-based conclusions using code diffs, runtime logs, and hardware test outcomes.
- **REQ-004**: Isolate the minimal regression-inducing behavior before proposing a permanent fix.
- **REQ-005**: Preserve existing project constraints for ESP8266, LittleFS, watchdog behavior, and HTTP-only OTA operation.
- **REQ-006**: Verify whether manual USB flashing succeeds with the same `.littlefs.bin` image that fails over OTA.
- **REQ-007**: Verify whether the problem is filesystem-only or also affects firmware OTA (`U_FLASH`).
- **SEC-001**: Do not introduce HTTPS or WSS into the debug approach; keep OTA and device checks HTTP-only per project rules.
- **CON-001**: Use actual ESP8266 hardware for final validation; simulator-only reasoning is insufficient for OTA/LittleFS behavior.
- **CON-002**: Do not assume current build outputs are valid until both firmware and LittleFS images are built from the same git revision.
- **CON-003**: Do not modify accepted ADR intent during debugging; this is an implementation/regression investigation, not an architecture rewrite.
- **CON-004**: Avoid destructive flash commands unless explicitly chosen for a controlled test case.
- **GUD-001**: Follow ADR-029 in [docs/adr/ADR-029-simple-xhr-ota-flash.md](docs/adr/ADR-029-simple-xhr-ota-flash.md) as the intended OTA UX/flow baseline.
- **GUD-002**: Treat [src/OTGW-firmware/OTGW-ModUpdateServer-impl.h](src/OTGW-firmware/OTGW-ModUpdateServer-impl.h) as the authoritative ESP OTA backend implementation.
- **GUD-003**: Treat [src/OTGW-firmware/updateServerHtml.h](src/OTGW-firmware/updateServerHtml.h) as the authoritative OTA browser flow implementation.
- **GUD-004**: Treat [src/OTGW-firmware/helperStuff.ino](src/OTGW-firmware/helperStuff.ino) and [src/OTGW-firmware/settingStuff.ino](src/OTGW-firmware/settingStuff.ino) as critical post-filesystem-flash recovery paths.
- **PAT-001**: Debug by binary isolation: first prove which phase fails, then disable one post-flash action at a time.
- **PAT-002**: Keep each experiment reversible and record exact branch, commit, image hash, and hardware result.

## 2. Implementation Steps

### Implementation Phase 1

- **GOAL-001**: Establish a reproducible baseline and confirm the failure boundary on real hardware.

| Task | Description | Completed | Date |
| -------- | --------------------- | --------- | ---------- |
| TASK-001 | Select one physical ESP8266 device and one stable browser; record board type, flash size, USB adapter, and whether the device currently boots after manual USB flashing. |  |  |
| TASK-002 | Build matched firmware and LittleFS artifacts from current `dev` using `python build.py`; record the exact git hash and artifact filenames produced in `build/`. |  |  |
| TASK-003 | Flash both current firmware and current LittleFS over USB using a known-good manual process; confirm that the device boots cleanly and serves `/api/v2/health`. This verifies the images themselves are not corrupt. |  |  |
| TASK-004 | Starting from the USB-good state, perform OTA LittleFS flash using `/update` in the web UI with the exact same `.littlefs.bin`; record whether the device fails during upload, hangs after upload, reboots but does not mount LittleFS, or never becomes healthy again. |  |  |
| TASK-005 | Repeat `TASK-004` three times on current `dev` to classify the failure as deterministic or flaky; record success/failure ratio and exact symptom for each run. |  |  |
| TASK-006 | Repeat the same OTA LittleFS test on `v1.2.0`, then on `v1.1.0`, then on `v1.0.0`, each time using firmware and LittleFS built from the same release tag; confirm the last known good release by hardware evidence instead of memory. |  |  |

### Implementation Phase 2

- **GOAL-002**: Identify the exact code path that differs between successful and failing releases.

| Task | Description | Completed | Date |
| -------- | --------------------- | --------- | ---------- |
| TASK-007 | Diff [src/OTGW-firmware/OTGW-ModUpdateServer-impl.h](src/OTGW-firmware/OTGW-ModUpdateServer-impl.h) across `v1.0.0`, `v1.1.0`, `v1.2.0`, and `dev`, focusing on `UPLOAD_FILE_START`, `UPLOAD_FILE_WRITE`, `UPLOAD_FILE_END`, `Update.begin(..., U_FS)`, `LittleFS.end()`, `LittleFS.begin()`, `updateLittleFSStatus()`, `writeSettings()`, and `ESP.restart()`. |  |  |
| TASK-008 | Diff [src/OTGW-firmware/updateServerHtml.h](src/OTGW-firmware/updateServerHtml.h) across the same releases, focusing on `XMLHttpRequest`, upload timeout handling, `waitForDeviceReboot()`, settings backup, Dallas label restore, and health-check polling. |  |  |
| TASK-009 | Diff [src/OTGW-firmware/helperStuff.ino](src/OTGW-firmware/helperStuff.ino) for `updateLittleFSStatus()` and any filesystem probe/write behavior introduced after earlier releases. |  |  |
| TASK-010 | Diff [src/OTGW-firmware/settingStuff.ino](src/OTGW-firmware/settingStuff.ino) for `writeSettings(bool show)`, especially file open/close ordering, `yield()` placement, and any code path that can execute immediately after OTA filesystem flash. |  |  |
| TASK-011 | Diff [src/OTGW-firmware/OTGW-firmware.ino](src/OTGW-firmware/OTGW-firmware.ino) for boot-time `LittleFS.begin()`, `readSettings(true)`, `checklittlefshash()`, and `doBackgroundTasks()` behavior while `state.flash.bESPactive` is true. |  |  |
| TASK-012 | Verify that [Makefile](Makefile), [build.py](build.py), and the effective FQBN remain compatible across the compared releases, especially `eesz=4M2M` and LittleFS image size `1024000`. |  |  |

### Implementation Phase 3

- **GOAL-003**: Instrument the current failing path so each phase boundary is observable on hardware.

| Task | Description | Completed | Date |
| -------- | --------------------- | --------- | ---------- |
| TASK-013 | Add temporary telnet debug markers in [src/OTGW-firmware/OTGW-ModUpdateServer-impl.h](src/OTGW-firmware/OTGW-ModUpdateServer-impl.h) at these exact boundaries: before `LittleFS.end()`, after `Update.end(true)`, before `LittleFS.begin()`, after `LittleFS.begin()`, before `updateLittleFSStatus(F("/.ota_post"))`, after it returns, before `writeSettings(false)`, after it returns, before `ESP.restart()`. |  |  |
| TASK-014 | Add temporary boot markers in [src/OTGW-firmware/OTGW-firmware.ino](src/OTGW-firmware/OTGW-firmware.ino) immediately before and after `LittleFS.begin()`, `readSettings(true)`, and `checklittlefshash()`. |  |  |
| TASK-015 | Add temporary failure markers in [src/OTGW-firmware/helperStuff.ino](src/OTGW-firmware/helperStuff.ino) inside `updateLittleFSStatus()` to log open failure, zero-byte write, and flush success/failure for the probe file. |  |  |
| TASK-016 | Add temporary failure markers in [src/OTGW-firmware/settingStuff.ino](src/OTGW-firmware/settingStuff.ino) around `LittleFS.open(SETTINGS_FILE, "w")`, the first `yield()`, JSON write completion, and `file.close()`. |  |  |
| TASK-017 | Capture telnet logs for one successful release and one failing release using the same OTA filesystem flash sequence and store both logs for side-by-side comparison. |  |  |

### Implementation Phase 4

- **GOAL-004**: Prove or disprove each suspected regression source by controlled experiments.

| Task | Description | Completed | Date |
| -------- | --------------------- | --------- | ---------- |
| TASK-018 | Create experiment build A: in [src/OTGW-firmware/OTGW-ModUpdateServer-impl.h](src/OTGW-firmware/OTGW-ModUpdateServer-impl.h), keep `LittleFS.begin()` but skip `updateLittleFSStatus()` and skip `writeSettings()` after filesystem flash; reboot immediately after `Update.end(true)`. Test OTA LittleFS flash three times. |  |  |
| TASK-019 | Create experiment build B: restore `updateLittleFSStatus()` only, still skip `writeSettings()`. Test OTA LittleFS flash three times. |  |  |
| TASK-020 | Create experiment build C: skip `updateLittleFSStatus()` but restore `writeSettings(false)`. Test OTA LittleFS flash three times. |  |  |
| TASK-021 | Create experiment build D: keep both current behaviors but disable health polling from [src/OTGW-firmware/updateServerHtml.h](src/OTGW-firmware/updateServerHtml.h) after upload completion to eliminate immediate post-reboot `/api/v2/health` writes during recovery. Test OTA LittleFS flash three times. |  |  |
| TASK-022 | Create experiment build E: change `writeSettings(false)` to a deferred boot-time restore marker instead of writing immediately post-flash. Test whether the device boots reliably and can then restore settings on first normal boot. |  |  |
| TASK-023 | For every experiment, record boot result, LittleFS mount result, `settings.ini` existence, `/version.hash` readability, and `/api/v2/health` response. |  |  |

### Implementation Phase 5

- **GOAL-005**: Validate the fix candidate and ensure no regression to intended OTA behavior.

| Task | Description | Completed | Date |
| -------- | --------------------- | --------- | ---------- |
| TASK-024 | Choose the smallest change set that converts the failing current build into a reliable build while preserving settings or replacing them with a safer restore strategy. |  |  |
| TASK-025 | Validate the selected fix on current `dev` with at least five consecutive OTA LittleFS flashes on the same hardware and at least one cold power-cycle after a successful OTA update. |  |  |
| TASK-026 | Validate firmware OTA (`U_FLASH`) still works after the fix by flashing a `.ino.bin` from `/update` and verifying the device returns healthy. |  |  |
| TASK-027 | Validate the web UI update page in [src/OTGW-firmware/updateServerHtml.h](src/OTGW-firmware/updateServerHtml.h) still performs the expected user-visible flow: upload progress, reboot wait, health confirmation, and redirect. |  |  |
| TASK-028 | Document the root cause, the verified fix, and any changed operational caveats in a review or fix document under `docs/reviews/` if the result is substantial. |  |  |

## 3. Alternatives

- **ALT-001**: Debug only by code inspection without hardware reproduction. Rejected because OTA/LittleFS failures are timing- and device-dependent.
- **ALT-002**: Immediately remove all post-flash restore logic permanently. Rejected because it may fix the symptom while dropping settings persistence behavior without proof.
- **ALT-003**: Assume the browser/XHR layer is the primary cause and debug only frontend code. Rejected because manual USB flashing works with the same image and the highest-risk code runs in the post-flash backend path.
- **ALT-004**: Change flash layout or partition size first. Rejected because `4M2M` and `1024000`-byte LittleFS generation are stable across the known-good older releases.

## 4. Dependencies

- **DEP-001**: Physical ESP8266 hardware with 4MB flash using the same board profile as current builds (`d1_mini`, `eesz=4M2M`).
- **DEP-002**: Valid build artifacts from `python build.py` for each compared tag or commit.
- **DEP-003**: Telnet logging access to observe OTA markers during and after upload.
- **DEP-004**: Git history access for `v1.0.0`, `v1.1.0`, `v1.2.0`, and current `dev`.
- **DEP-005**: Browser access to `/update` on the target device.
- **DEP-006**: Reliable manual USB flash path for baseline recovery between failed OTA tests.

## 5. Files

- **FILE-001**: [src/OTGW-firmware/OTGW-ModUpdateServer-impl.h](src/OTGW-firmware/OTGW-ModUpdateServer-impl.h) — OTA upload handler, filesystem target handling, post-flash restore, reboot.
- **FILE-002**: [src/OTGW-firmware/updateServerHtml.h](src/OTGW-firmware/updateServerHtml.h) — browser upload flow, backup flow, reboot wait, health polling.
- **FILE-003**: [src/OTGW-firmware/helperStuff.ino](src/OTGW-firmware/helperStuff.ino) — `updateLittleFSStatus()` filesystem probe and post-boot LittleFS validation.
- **FILE-004**: [src/OTGW-firmware/settingStuff.ino](src/OTGW-firmware/settingStuff.ino) — `writeSettings(bool show)` immediate post-flash settings rewrite behavior.
- **FILE-005**: [src/OTGW-firmware/OTGW-firmware.ino](src/OTGW-firmware/OTGW-firmware.ino) — boot sequence, LittleFS mount, flash-mode background task restrictions.
- **FILE-006**: [Makefile](Makefile) — FQBN and LittleFS image generation settings.
- **FILE-007**: [build.py](build.py) — build orchestration for firmware and filesystem image generation.
- **FILE-008**: [docs/adr/ADR-029-simple-xhr-ota-flash.md](docs/adr/ADR-029-simple-xhr-ota-flash.md) — intended OTA behavior baseline.
- **FILE-009**: [docs/SAFARI_FLASH_FIX.md](docs/SAFARI_FLASH_FIX.md) — prior OTA troubleshooting context and browser-specific history.

## 6. Testing

- **TEST-001**: USB baseline test: manual flash of firmware + LittleFS from the same commit must boot successfully and return `health.status === 'UP'`.
- **TEST-002**: OTA LittleFS regression reproduction on current `dev`: run three to five consecutive OTA filesystem flashes and classify outcomes.
- **TEST-003**: Historical comparison test on `v1.0.0`, `v1.1.0`, and `v1.2.0`: identify the last known good release by real hardware result.
- **TEST-004**: Post-flash instrumentation log test: verify each marker around `LittleFS.begin()`, `updateLittleFSStatus()`, `writeSettings()`, and `ESP.restart()` is emitted in order.
- **TEST-005**: Experiment A-E comparison: determine which post-flash action first reintroduces the failure.
- **TEST-006**: First-boot verification after OTA: confirm `LittleFS.begin()`, `readSettings(true)`, and `checklittlefshash()` complete without boot loop or mount failure.
- **TEST-007**: Health endpoint behavior test after OTA: determine whether immediate `/api/v2/health` polling worsens recovery by writing to a fresh filesystem too early.
- **TEST-008**: Firmware OTA control test: confirm `.ino.bin` OTA remains functional after the selected fix.

## 7. Risks & Assumptions

- **RISK-001**: The failure may be timing-sensitive and appear flaky, requiring repeated runs to distinguish cause from noise.
- **RISK-002**: Telnet logging may be incomplete near reboot boundaries; some failure states may require serial-adapter observation or persisted reboot logs.
- **RISK-003**: A post-flash settings rewrite may succeed on some boards and fail on others due to flash wear, timing, or supply stability.
- **RISK-004**: Browser behavior may still contribute, so one browser must be held constant while backend experiments are performed.
- **ASSUMPTION-001**: Manual USB flashing proves the generated `.littlefs.bin` image is structurally valid when written outside the OTA path.
- **ASSUMPTION-002**: The most likely regression source is not the raw `Update.begin(..., U_FS)` call itself but the immediate post-flash filesystem activity before or immediately after reboot.
- **ASSUMPTION-003**: The current filesystem payload size increase raises stress on an older OTA recovery design even if the nominal partition size is unchanged.

## 8. Related Specifications / Further Reading

[docs/adr/ADR-029-simple-xhr-ota-flash.md](docs/adr/ADR-029-simple-xhr-ota-flash.md)
[docs/SAFARI_FLASH_FIX.md](docs/SAFARI_FLASH_FIX.md)
[docs/FLASH_GUIDE.md](docs/FLASH_GUIDE.md)
[docs/api/README.md](docs/api/README.md)