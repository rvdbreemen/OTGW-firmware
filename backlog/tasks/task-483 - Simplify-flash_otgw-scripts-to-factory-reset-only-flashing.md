---
id: TASK-483
title: Simplify flash_otgw scripts to factory-reset-only flashing
status: Done
assignee:
  - '@codex'
created_date: '2026-04-29 22:10'
updated_date: '2026-04-30 00:29'
labels:
  - flash scripts
dependencies: []
ordinal: 1000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Remove the flash helper complexity added for upgrade and settings preservation. The Windows and Linux/macOS helpers should expose one normal flashing behavior: factory reset by default, flashing firmware and filesystem without backup/restore or alternate upgrade modes.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 flash_otgw.bat defaults to a factory-reset full image flash with no interactive mode chooser or settings backup/restore prompt.
- [x] #2 flash_otgw.sh defaults to a factory-reset full image flash with no interactive mode chooser or settings backup/restore prompt.
- [x] #3 The helper help text and accepted options no longer advertise upgrade, preserve-settings, or alternate filesystem-preserving modes.
- [x] #4 Targeted syntax/help checks pass for both scripts.
- [x] #5 Distribution README/manual references no longer tell users to use flash_otgw upgrade or firmware-only flashing modes.
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Remove upgrade/factory/erase/preserve mode parsing and backup/restore helpers from flash_otgw.bat and flash_otgw.sh. 2. Make the default bin selection require the merged-full image unless --bin is supplied. 3. Keep only targeting options: --port, --bin, --board, --baud, and --help. 4. Update ready/complete output to state factory-reset behavior. 5. Remove stale --upgrade/firmware-only flash instructions from distribution README/manual text. 6. Run batch help, shell syntax, README generation, build, evaluator/unit, and diff whitespace checks.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Simplified flash_otgw.bat and flash_otgw.sh to a single factory-reset path: accepted options are now only --port, --bin, --board, --baud, and --help; default bin selection requires merged-full; write_flash uses -z -e at 0x0.

Updated build.py distribution packaging and generated README text so release flash zips package only merged-full plus flash scripts/readmes and no longer advertise --upgrade, preserve-settings, firmware-only USB modes, backup/restore, or YES confirmation.

Updated the English and Dutch hardware manual ESP32-S3 merged-binary note to describe merged-full/factory-reset script behavior instead of auto-selecting firmware-only variants.

Verification: cmd /c flash_otgw.bat --help passed; Git Bash bash -n flash_otgw.sh passed; Git Bash flash_otgw.sh --help passed; removed --upgrade flag is rejected by both scripts; git diff --check passed with only CRLF normalization warnings; tests/test_build.py passed; tests/test_evaluate.py passed; build.py passed for ESP8266 and ESP32-S3 and rebuilt distribution zips.

Zip verification: rebuilt ESP8266 and ESP32 flash zips contain only merged-full bin, flash_otgw.bat, flash_otgw.sh, README.txt, and README_NL.txt. Extracted readmes had no matches for --upgrade, --erase, preserve, backup, restore, firmware-only, alleen-firmware, YES, Three flash, or Drie flash.

Checklist caveat: evaluate.py --quick still exits 1 on the existing 15 PROGMEM violations, with ADR-062 mqtt_configuratie.cpp not found and sendMQTTheapdiag buffer arithmetic warnings. This is unchanged project debt outside TASK-483 scope.

Post-closure bug report: running build\flash_otgw.bat with no arguments failed before flashing with "The system cannot find the batch label specified - args_done". Reopened TASK-483 to fix the Windows no-argument path and packaging output.

Windows batch bug fix: removed the no-argument forward goto to :args_done and replaced it with a parser subroutine that is only called when arguments exist. Added an errorlevel check after call :select_bin so missing-bin failures stop immediately instead of continuing into board detection.

Packaging hardening: build.py now writes flash_otgw.bat with CRLF line endings when copying to build/ and when embedding it in distribution zips, so cmd.exe behavior does not depend on Git checkout line endings.

Verification after bug report: safe no-argument batch smoke tests from extracted ESP8266 and ESP32 zip batch files no longer hit the batch-label error; they reach the expected No merged-full bin found error when the test intentionally omits binaries. build/flash_otgw.bat --help passed, rg found no args_done references, bash -n flash_otgw.sh passed, git diff --check passed with only CRLF normalization warnings, tests/test_build.py passed, and a clean full build.py completed successfully for ESP8266 and ESP32-S3 after stopping stale PlatformIO child processes and running build.py --clean.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Simplified the cross-platform OTGW flash helpers to one user-facing path: factory reset. flash_otgw.bat and flash_otgw.sh now accept only targeting/help options, auto-select merged-full images by default, erase flash with write_flash -z -e, and no longer offer mode selection, --upgrade/--factory/--erase flags, YES confirmation, preserve-settings prompts, or HTTP backup/restore. Fixed the reported Windows no-argument batch failure by removing the fragile args_done forward-goto parser, adding a parser subroutine and select_bin error handling, and hardening build.py to emit CRLF batch files in build/ and distribution zips. Verification passed for batch help, safe no-argument smoke tests from extracted zip batch files, removed option rejection, zip contents/readme scan, tests/test_build.py, and a clean full build.py for ESP8266 and ESP32-S3. evaluate.py --quick remains red on existing unrelated PROGMEM debt and two warnings, documented in implementation notes.
<!-- SECTION:FINAL_SUMMARY:END -->
