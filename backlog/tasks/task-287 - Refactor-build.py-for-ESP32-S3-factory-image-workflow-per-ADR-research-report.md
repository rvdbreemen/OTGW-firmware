---
id: TASK-287
title: >-
  Refactor build.py for ESP32-S3 factory-image workflow (per ADR/research
  report)
status: To Do
assignee: []
created_date: '2026-04-17 22:42'
labels:
  - build
  - esp32
  - refactor
dependencies:
  - TASK-286
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The deep-research flash-esp32 report (log-issues/deep-research-report-flash-esp32-solution.md) recommends a single-factory-app layout on 4MB flash with 2MB LittleFS, arduino-cli as primary backend (core 3.3.x follows Arduino-ESP32 directly; PlatformIO stable espressif32 still on Arduino 2.0.17), and three deliverables per release: app.bin + littlefs.bin + initial-4mb.bin (merged bootloader + partitions + app + fs). Current build.py was ESP8266-first; ESP32-S3 is bolted on and has no factory-image merge, no reproducible env, no ccache.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 build.py accepts --target {esp8266,esp32s3}, --tool {arduino-cli,platformio}, --factory-image, --ccache, --manifest flags
- [ ] #2 Stable cache roots under .cache/arduino, .cache/pio, .cache/ccache are created and reused across runs
- [ ] #3 Reproducible-build env (LC_ALL=C, LANG=C, TZ=UTC, PYTHONHASHSEED=0, SOURCE_DATE_EPOCH) is applied consistently
- [ ] #4 ESP32-S3 arduino-cli build produces app.bin + littlefs.bin + bootloader.bin + partitions.bin artifacts under dist/
- [ ] #5 --factory-image produces dist/otgw-esp32s3-initial-4mb.bin via esptool merge_bin with correct offsets (0x0 bootloader, 0x8000 partitions, 0x10000 app, 0x200000 littlefs)
- [ ] #6 --manifest writes dist/manifest.sha256.json with sha256 of each artifact
- [ ] #7 ESP8266 legacy build path still works (no regression)
- [ ] #8 ADR update or new ADR covering the build.py architecture change
<!-- AC:END -->
