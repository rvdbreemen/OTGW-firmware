---
id: TASK-865.1
title: 'feat(build): remove the esp8266 PlatformIO env and build-target plumbing'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-13 05:41'
updated_date: '2026-06-21 07:07'
labels:
  - async-esp32s3
dependencies: []
parent_task_id: TASK-865
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-128, drop-esp8266 build-side)
Drop the ESP8266 build target so 2.0.0 builds only ESP32-S3: `esp32` (OTGW32/OTDirect), `esp32-classic` (S3 mini in Classic socket/PIC), `esp32-combo` (ADR-127). Build-config/CI/release-helpers only; no source `#if ESP8266` (that is the code-cut task).

## What (verified anchors)
- platformio.ini: delete `[env:esp8266]` (45-112); `default_envs = esp8266`->`esp32` (13); header comment (1-8); reword esp32-classic/combo "mirrors the ESP8266 env" comments.
- build.py: delete `TARGETS["esp8266"]` (58-74), `PIO_ENV_MAP["esp8266"]` (1872); argparse `--target` choices/help (2413/2415); collapse esp8266 single-merge branch (2576-2585) to the ESP32 bootloader_offset path; doc/README-gen strings (15,851,1103,1120,1251,1353,1589,2317).
- .github/workflows/build.yml: matrix `env:[esp8266,esp32]`->`[esp32,esp32-classic,esp32-combo]` (25) (closes the classic/combo CI gap); comment (5).
- .github/workflows/dependency-scan.yml: `-e nodemcuv2 -e esp32`->`-e esp32 -e esp32-classic -e esp32-combo` (52,57).
- scripts/patch_pio_libs.py: drop Patch1 (80-93) + Patch2 (109-128); KEEP mklittlefs PATH prepend (43-55).
- flash_otgw.sh/.bat: remove `--board esp8266` + `*-esp8266-*` autodetect; keep esp32.

## Acceptance Criteria
- build: grep finds no `[env:esp8266]`/`TARGETS["esp8266"]`/`PIO_ENV_MAP["esp8266"]`.
- build: `pio run -e esp32|-e esp32-classic|-e esp32-combo` each exit 0 + firmware.bin.
- build: bare `pio run` no 'unknown environment esp8266'.
- build: `python build.py` emits flash zips for esp32/esp32-classic/esp32-combo only (grep per-env SUCCESS line; build.py masks per-env failures).
- build: `--target esp8266` rejected; build.yml matrix = 3 esp32 targets; dependency-scan no `nodemcuv2`.
- evaluator: `python evaluate.py --quick` no new failures.
- field: flash esp32-classic + esp32-combo merged-full.bin on S3 hardware, boot+AP+web UI (merged offsets changed).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Build-plumbing esp8266 removal implemented: platformio.ini ([env:esp8266] deleted, default_envs=esp32, header+classic/combo comments reworded), build.py (TARGETS/PIO_ENV_MAP/argparse/merge-branch/README-gen all de-esp8266'd), scripts/patch_pio_libs.py (Patch1+Patch2 dropped, orphaned patch_file helper removed, Patch3 mklittlefs PATH kept), .github/workflows/build.yml (matrix=[esp32,esp32-classic,esp32-combo]), dependency-scan.yml (-e esp32 -e esp32-classic -e esp32-combo), flash_otgw.sh/.bat (esp8266 board+autodetect removed). Evaluator: 61 pass / 2 warn (baseline) / 0 fail — no new failures. PlatformIO config parse: exactly 3 envs (esp32/esp32-classic/esp32-combo), no esp8266. --target esp8266 rejected (argparse exit 2). Full build running.

VERIFIED: python build.py (all targets, single sequential process) emitted 6 per-env SUCCESS lines (esp32/esp32-classic/esp32-combo firmware ~7min + filesystem ~40s each), 0 FAILED, 0 esp8266 references in log. Three flash zips only: esp32-otgw32, esp32-classic, esp32-combo (no esp8266 zip). firmware.bin present per env (1.89/1.84/1.94 MB). PlatformIO config parse reports exactly 3 envs. --target esp8266 rejected (argparse exit 2). evaluate.py --quick: 61 pass / 2 warn (unchanged baseline: ESP-abstraction-baseline-1, STATUS_BURST_COOLDOWN boards.h-not-found) / 0 fail — no NEW failures. Forbidden-token grep ([env:esp8266], TARGETS[esp8266], PIO_ENV_MAP[esp8266]) empty. build.yml matrix=[esp32,esp32-classic,esp32-combo]; dependency-scan nodemcuv2 count=0. Field-validation AC (flash on S3 HW) out of scope (no hardware). Not committed/bumped/status-changed — Land phase.

Broad host-side sweep (.github, bin, scripts, top-level tooling) found two residual esp8266 references OUTSIDE this task's anchor list, both deliberately deferred: (1) flash_esp.py carries an esp8266 board dict + --board esp8266 — it is a standalone developer flasher (downloads/flashes a published release), NOT wired into build.py/CI/dist-zip (dist zip bundles flash_otgw.sh/.bat only) and its bare 'esp8266': key does not match the AC grep tokens; recommend folding into the code-cut task or a follow-up. (2) scripts/fix_satcycles.py is a source-fixture generator, not build plumbing. evaluate.py esp8266 hits are the evaluator's own source-memory-model checks (code-cut task domain). Also fixed one in-scope stale comment: platformio.ini esp32 lib_ignore 'mirrors the ESP8266' reworded (comment-only, no rebuild needed).

Landed in commit b9263957 (pushed to origin/feature-2.0.0-esp32s3-async). esp8266 PlatformIO env + build-target plumbing removed: platformio.ini parses to exactly 3 envs (esp32, esp32-classic, esp32-combo), default_envs=esp32; build.py, build.yml, dependency-scan.yml, flash_otgw.sh/.bat, patch_pio_libs.py de-esp8266'd. Build/evaluator-verifiable ACs PASS first-hand: all 3 esp32* envs compile (honest per-env exit 0, firmware.bin present), --target esp8266 rejected by argparse, forbidden esp8266 tokens absent, evaluate.py --quick 61 pass / 2 pre-existing-baseline warn / 0 fail. version.h/.hash build churn deliberately NOT committed (bump-owned as a unit, no prerelease bump warranted: srcTouched=false). Scope-guard: .claude/workflows+skills implement-next-task changes excluded (orchestrator-harness, not task content). REMAINING FIELD-VALIDATION AC (cannot self-certify, no hardware): flash esp32-classic + esp32-combo merged-full.bin on S3 hardware, confirm boot + AP + web UI (merged offsets changed). Reported as In Review (project backlog schema has no In Review state; on-disk status stays In Progress) pending that hardware sign-off.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Removed the esp8266 PlatformIO env + build-target plumbing (ADR-128); 2.0.0 dev builds ESP32 targets only. Live on dev, alpha-validated. Closed per migration-accepted sign-off (Robert 2026-06-21).
<!-- SECTION:FINAL_SUMMARY:END -->
