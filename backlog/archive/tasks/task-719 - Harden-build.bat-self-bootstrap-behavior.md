---
id: TASK-719
title: Harden build.bat self-bootstrap behavior
status: Done
assignee:
  - '@claude'
created_date: '2026-05-26 16:30'
updated_date: '2026-05-26 17:15'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Run build.bat on Windows, fix any failures, and ensure the script fully bootstraps required tooling/dependencies without user intervention.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.bat runs from a clean Windows environment without manual installs
- [x] #2 Any missing dependency is auto-installed or clearly auto-downloaded by script
- [x] #3 Firmware+filesystem image build completes successfully
- [x] #4 Documented fixes and validation are recorded in task notes
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Execute build.bat in current Windows shell and capture first failing step.\n2. Trace the failing bootstrap/tooling path in build.bat and build.py.\n3. Patch scripts so missing prerequisites are auto-downloaded/installed without user prompts.\n4. Re-run build.bat until firmware+filesystem build succeeds.\n5. Record fixes and verification in task notes and AC checks.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Validated and fixed build flow on Windows.

Fixes implemented:
- build.bat now bootstraps portable Python 3.12.10 automatically when no system Python/.venv is available.
- build.bat ensures embedded Python can import local repo modules by updating python*._pth and PYTHONPATH.
- build.py check_esptool() now preinstalls setuptools/wheel and uses a more reliable pip install strategy with diagnostics.
- build.py now collects PlatformIO artifacts in phases (firmware first, filesystem second) to avoid Windows .pio cleanup races deleting firmware.bin before merge step.

Validation:
- build.bat --target esp32 completed successfully (exit code 0).
- Build artifacts include firmware/littlefs/merged binaries and flash zip outputs for esp32 and esp8266 in build/.

Final verification: build.bat --target esp8266 and build.bat --target esp32 both completed successfully with merged binaries and flash zips generated.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Hardened Windows build bootstrap and artifact handling so build.bat is self-contained and can recover missing prerequisites without manual setup. Added portable Python bootstrap in build.bat, improved esptool dependency bootstrapping in build.py, and fixed phased artifact collection to prevent firmware artifact loss during buildfs cleanup on Windows. Verified with successful build.bat --target esp32 run producing firmware, littlefs, merged binaries, and distribution zips.
<!-- SECTION:FINAL_SUMMARY:END -->
