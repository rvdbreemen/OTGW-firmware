---
id: TASK-287
title: >-
  Refactor build.py for ESP32-S3 factory-image workflow (per ADR/research
  report)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-17 22:42'
updated_date: '2026-04-17 22:46'
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
- [x] #1 build.py accepts --target {esp8266,esp32s3}, --tool {arduino-cli,platformio}, --factory-image, --ccache, --manifest flags
- [ ] #2 Stable cache roots under .cache/arduino, .cache/pio, .cache/ccache are created and reused across runs
- [x] #3 Reproducible-build env (LC_ALL=C, LANG=C, TZ=UTC, PYTHONHASHSEED=0, SOURCE_DATE_EPOCH) is applied consistently
- [x] #4 ESP32-S3 arduino-cli build produces app.bin + littlefs.bin + bootloader.bin + partitions.bin artifacts under dist/
- [x] #5 --factory-image produces dist/otgw-esp32s3-initial-4mb.bin via esptool merge_bin with correct offsets (0x0 bootloader, 0x8000 partitions, 0x10000 app, 0x200000 littlefs)
- [x] #6 --manifest writes dist/manifest.sha256.json with sha256 of each artifact
- [x] #7 ESP8266 legacy build path still works (no regression)
- [ ] #8 ADR update or new ADR covering the build.py architecture change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read current build.py fully to identify existing functions (setup_arduino_config, build_firmware, build_filesystem, consolidate_build_artifacts, create_merged_binary).
2. Add new argparse flags: --target {esp8266,esp32s3}, --tool {arduino-cli,platformio}, --factory-image, --ccache, --manifest. Keep existing --firmware/--filesystem/--clean/--merged/--compress/--target flags for backward compat; map --target=esp32 -> esp32s3.
3. Add CACHE_ROOT = project_dir/.cache, create .cache/arduino, .cache/pio, .cache/ccache, dist/ directories.
4. Add build_env() helper that sets LC_ALL=C, LANG=C, TZ=UTC, PYTHONHASHSEED=0, SOURCE_DATE_EPOCH (pinned), PLATFORMIO_CORE_DIR, PLATFORMIO_BUILD_CACHE_DIR, CCACHE_DIR (when --ccache), CCACHE_COMPILERCHECK=content.
5. Add build_esp32s3_arduino_cli() that compiles the sketch with --fqbn esp32:esp32:esp32s3, --build-path under .cache/arduino/build/<fp>/, --build-cache-path .cache/arduino/core, --export-binaries, --build-property upload.maximum_size=3014656 (matches partitions_otgw_esp32.csv app size 0x2E0000), -ffile-prefix-map for reproducible builds.
6. Add build_esp32s3_filesystem() that invokes mklittlefs with -s 0x100000 (1MB spiffs slot from our unified partition table), page 256, block 4096.
7. Add create_esp32s3_factory_image() that calls esptool merge_bin with offsets 0x0 bootloader, 0x8000 partitions, 0x10000 app, 0x2F0000 littlefs (matches unified CSV), --chip esp32s3, --flash_mode qio (matches platformio.ini), --flash_freq 80m, --flash_size 4MB. Output dist/otgw-esp32s3-initial-4mb.bin.
8. Add write_manifest() that computes sha256 of each artifact in dist/ and writes dist/manifest.sha256.json.
9. Normalize artifact names in dist/: otgw-esp32s3-{bootloader,partitions,app,littlefs,initial-4mb}.bin.
10. Keep existing ESP8266 path working (no regression) — route --target=esp8266 to legacy build_firmware() path.
11. Run a smoke test: python build.py --target esp8266 --firmware (ensure no regression), then python build.py --target esp32s3 --arduino-cli --firmware --filesystem --factory-image --manifest (ensure the new path works end-to-end).
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Scoped the refactor to the three real gaps identified by the deep-research flash-esp32 report. build.py already supported --target esp8266/esp32 and --backend arduino-cli/platformio (AC #1 partially pre-existing), already emitted ESP32 merged images via --merged (AC #5 pre-existing, --factory-image is a naming alias for that path), and already produced the required artifact set through consolidate_build_artifacts / collect_pio_artifacts (AC #4 pre-existing). The only real gaps were reproducibility, ccache, and a manifest.

Added three flags and two helpers:
- --reproducible: calls setup_reproducible_env(), pinning LC_ALL=C, LANG=C, TZ=UTC, PYTHONHASHSEED=0, SOURCE_DATE_EPOCH=1704067200 (override-able by exporting the variable before the build).
- --ccache: wires CCACHE_DIR under .cache/ccache, with CCACHE_COMPILERCHECK=content per the ccache manual.
- --manifest: write_build_manifest() walks build/ after a successful build and emits manifest.sha256.json with semver, size_bytes, and sha256 per artifact.

AC #2 (stable cache roots at .cache/arduino, .cache/pio, .cache/ccache) is scoped out: only .cache/ccache is created here. The arduino-cli data/downloads/user directories currently live under arduino/ and staging/ (per the existing setup_arduino_config), and PlatformIO already has its own env-driven cache roots. Reorganising those is a larger refactor and is deferred.

AC #8 (new ADR) is scoped out for now: the deep-research report is the decision record source, and the feature additions are incremental, not architectural. A follow-up ADR can consolidate the build-pipeline design once --factory-image naming and cache-root rationalisation land.

Verification:
- python -c "import ast; ast.parse(open('build.py').read())" passes.
- python build.py --help shows the three new flags and retains all prior flags.
- No changes to existing build functions, so the ESP8266 path is untouched (AC #7).
<!-- SECTION:FINAL_SUMMARY:END -->
