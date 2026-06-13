---
id: TASK-865.1
title: 'feat(build): remove the esp8266 PlatformIO env and build-target plumbing'
status: To Do
assignee: []
created_date: '2026-06-13 05:41'
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
