# Cerebrum

> OpenWolf's learning memory. Updated automatically as the AI learns from interactions.
> Do not edit manually unless correcting an error.
> Last updated: 2026-05-01

## User Preferences

<!-- How the user likes things done. Code style, tools, patterns, communication. -->

## Key Learnings

- **Settings page layout (DS:SETTINGS-GROUP):** `.settings-group-body` is a 2-col grid (`var(--settings-label-w, max-content) 1fr`); rows subgrid into it. Anything appended into a `.settings-group-body` becomes a grid item — wide content (e.g. the WiFi-scan panel's full-width `<p>`) inflates the shared col-1 `max-content` track and stretches every label. Full-width sub-panels MUST use `grid-column: 1 / -1` (like `.fixed-ip-section` and now `#wifi-scan-panel`). And `normalizeSettingsLabelWidth()` must measure ONLY real row labels (`.settings-group-body .settingDiv > .settings-field-container`), never the broad descendant selector. (TASK-763, bug-076)
- **Project:** OTGW-firmware
- **Description:** [![Join the Discord chat](https://img.shields.io/discord/812969634638725140.svg?style=flat-square)](https://discord.gg/zjW3ju7vGQ)
- On the 2.0.0 branch, `hd_drip_cooldown_skip` is a post-status-burst pacing counter from `MQTTstuff.ino`, not a heap-pressure counter; the actual heap-pressure counter is `hd_drip_slowmode`.
- On ESP32, `platformMaxFreeBlock()` uses `ESP.getMaxAllocHeap()` as a live runtime value. Docs and diagnostics should describe it as the largest allocatable block, not with the ESP8266-specific `getMaxFreeBlockSize()` wording.
- **Removing a published REST/MQTT field is coupled to its frontend consumer.** Dropping `picavailable` from `/api/v2/device/info` (ADR-113 stage 2, TASK-754) silently breaks `index.js` if not migrated in the SAME build: `picAvailable = !!d.picavailable` becomes `false`, and the `#tabPICflash` route gated on it makes the PIC-flash page unreachable. Backend field removal + frontend rewrite must ship atomically (one `build.py` → firmware + LittleFS). Split a deprecation removal into "non-coupled (MQTT publish, HA discovery) now" vs "coupled (REST field + JS consumer) atomically" — never ship the REST half alone.
- **The HA discovery sensor loop bound is exact, not slack.** `doAutoConfigureMsgid()` iterates `while (sIdx < MQTT_HA_SENSOR_COUNT)` and breaks on id-change, so `MQTT_HA_SENSOR_COUNT` MUST equal the row count of `mqttHaSensors[]`. If COUNT < rows, the trailing OT-id's entries are silently never discovered (found id-254 flame_status dark this way, bug-088). When adding/removing a row, update COUNT and the `mqttHaSensorIndex[]` offsets together; `evaluate.py check_ha_sensor_index_consistency` validates the index but NOT the COUNT.

## Do-Not-Repeat

<!-- Mistakes made and corrected. Each entry prevents the same mistake recurring. -->
- **2026-05-26**: For WSL/Linux build reliability, do not assume system Python/pip exists or that package-manager installs are acceptable. `build.sh` must self-bootstrap an isolated local Python runtime and pip non-interactively when needed.
- **2026-05-26**: Version bump in 2.0.0 src files (comment headers) from a `build.py` run lands as unstaged changes. The pre-commit hook blocks the commit with "does not bump _VERSION_PRERELEASE" even when the version was already bumped in a previous commit. Use `OTGW_BUMP_HOOK_DISABLE=1 git commit` with a `chore(meta):` prefix to commit the housekeeping header updates. The commit-msg hook requires either a TASK-NNN reference or an exemption prefix (`chore(meta):`, `chore(release):`, `chore(housekeeping):`, `chore(daily-report):`).
- **2026-05-26**: The backlog CLI auto-stages and auto-commits task file changes. The adr-kit pre-commit hook can auto-change ADR Status from Proposed to Accepted. Do not manually re-write or re-commit ADR files after `backlog task edit` — check git log first to avoid duplicating the commit.
- **2026-05-26**: On Windows, overlapping `build.bat`/`pio` runs can leave locked `.pio\build` artifacts and cause false missing-bin/link errors. Before concluding there is a source regression, ensure only one PlatformIO process is active and rerun once clean.
- **2026-05-30**: OTGWSerial is an INDEPENDENT external/vendored library (src/libraries/OTGWSerial) and must NOT be pulled into the firmware platform abstraction. Do NOT add platform*() shims into it (no platformPicSerialBegin), do NOT remove its internal `#if defined(ESP8266)/(ESP32)` — those are library-internal and legitimate, exactly like a third-party lib. It belongs in evaluate.py's ESP_ABSTRACTION_EXCLUDED_LIB_DIRS (like SimpleTelnet). Maintainer directive during TASK-743 Tier 3: the ESP-abstraction rule governs APPLICATION code only, never the vendored libraries. (Voids TASK-743 AC#4.)
- **2026-05-29**: Do NOT run `pio run -e esp8266` and `pio run -e esp32` in parallel in the same worktree to "build in parallel". The arduino sketch-concat step (.ino -> .ino.cpp) shares an intermediate across envs even though each env has its own `.pio/build/<env>` dir; concurrent runs race and one env fails with `OTGW-firmware.ino.cpp: No such file or directory`. Firmware builds must be serial per tree; only `buildfs` (filesystem, no concat) parallelizes safely. For true parallel firmware builds use separate git worktrees. Recovery: `rm -rf .pio/build/<env>` then rebuild that env solo. See buglog bug-073.
<!-- Format: [YYYY-MM-DD] Description of what went wrong and what to do instead. -->

## Decision Log

- 2026-05-05: MQTT discovery drip policy is platform-aware on 2.0.0. ESP8266 keeps the existing `HEAP_LOW` / 2000ms cooldown behavior; ESP32 uses a shorter status-burst cooldown and only enters discovery slow-mode when both free heap and largest allocatable block are genuinely low.

<!-- Significant technical decisions with rationale. Why X was chosen over Y. -->
