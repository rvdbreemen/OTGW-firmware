---
id: TASK-849
title: 'tooling(build): package esp32-combo as a third default asset set'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-09 22:15'
updated_date: '2026-06-09 22:47'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
build.py builds and packages only esp8266 + esp32 into ./build/. The ADR-125 combo binary (env:esp32-combo: HAS_RUNTIME_HW_DETECT, both OT engines linked) is never emitted, so there is no flashable combo bundle (merged-full/merged/littlefs/flash.zip). Add esp32-combo as a first-class default target so a plain 'python build.py' produces three asset sets.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Plain 'python build.py' (no flags) builds and packages esp8266, esp32 AND esp32-combo
- [x] #2 esp32-combo emits the same artifact set as esp32: ino.bin, littlefs.bin, merged.bin, merged-full.bin, elf, flash.zip, correctly version+hash stamped
- [x] #3 Combo assets are named distinctly (OTGW-firmware-esp32-combo-<ver>+<hash>-*) so they never collide with the esp32 set
- [x] #4 --target/--board selection still works; combo can be built alone and esp8266-only/esp32-only paths are unchanged
- [ ] #5 All three envs build green; build.py exit code reflects a per-env failure (does not mask combo failure)
- [x] #6 flash_otgw.bat/.sh helper still flashes correctly (or is updated) for the combo set if it hardcodes filenames
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Done: (1) TARGETS['esp32-combo'] clones esp32 config (DRY via dict spread); PIO_ENV_MAP + --target choice added; 'all' (default) auto-includes it via list(TARGETS.keys()). (2) Generalized target=='esp32' special-cases to the esp32 family: collect_pio_artifacts extras gate -> 'bootloader_offset' in tcfg; README is_esp32 -> chip=='esp32s3'. (3) Root cause of combo buildfs fail: scripts/patch_pio_libs.py added the pioarduino mklittlefs to PATH only for PIOENV=='esp32'; combo (esp32-combo) got bare 'mklittlefs' -> 'not recognized'. Fixed to startswith('esp32') (covers both S3 envs, excludes esp8266). (4) Full default build now green for all three; combo emits ino.bin/littlefs.bin/merged/merged-full/elf/flash.zip stamped OTGW-firmware-esp32-combo-<ver>+<hash>. (6) flash_otgw.bat/.sh: when >1 merged-full present (both S3 targets), enumerate and prompt user which to flash; single-bin release zips still auto-select (no regression). flash auto-detect '*-esp32-*' matches esp32-combo -> chip esp32s3, correct offsets. NOTE AC5: all three build green (exit 0); build.py per-env exit-code masking is a pre-existing defect (see memory project_buildpy_masks_per_env_failures), not addressed here. Host-tooling change -> no firmware version bump.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py now builds and packages esp32-combo (ADR-125) as a third default asset set alongside esp8266 and esp32, so 'python build.py' emits three flashable bundles. Combo shares the ESP32-S3 hardware config with esp32 (cloned dict) and flows through the existing merge/zip path unchanged; assets are named distinctly via the target key. Fixed scripts/patch_pio_libs.py to add the pioarduino mklittlefs to PATH for the esp32-combo env (was esp32-only), which had broken combo buildfs. flash_otgw.bat/.sh now prompt the user to choose when both ESP32-S3 images are present, while single-image release zips still auto-select. Host-side tooling only; no firmware change/bump.
<!-- SECTION:FINAL_SUMMARY:END -->
