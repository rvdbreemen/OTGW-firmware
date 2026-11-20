---
id: TASK-2917
title: >-
  Capture tool: the flash zip lacks scripts/sat_log_to_csv.py, so the telnet.log
  CSV conversion fails on every OS
status: Done
assignee:
  - '@5e'
created_date: '2026-11-19T13:57:00Z'
updated_date: '2026-11-19T22:42:00Z'
labels:
  - bug
  - capture-tool
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1054000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
otgw_capture.py imports sat_log_to_csv (scripts/otgw_capture.py:250) for the telnet.log -> sattick.csv conversion, but build.py packs only otgw_capture.py + the capture launchers + the paho wheel into the flash zip (build.py:1290-1313). Every field capture logs 'csv conversion of telnet.log failed, the raw log is zipped anyway: ModuleNotFoundError: No module named sat_log_to_csv' (indigo_light Windows 24 h run beta.360 18/11, macOS run beta.382 19/11, #beta-testing 1572966038965063826). The raw telnet.log is still complete, so no data is lost; the CSV is a convenience. Found by comms 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py packs scripts/sat_log_to_csv.py next to otgw_capture.py in the flash zip (zipfile listing of a fresh build shows it)
- [x] #2 A test fails on the old build.py and passes on the fix (the packed file list contains every module otgw_capture.py imports from scripts/)
- [x] #3 A capture run from the new zip writes sattick.csv without the ModuleNotFoundError line in the transcript
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 5e: build.py now reads otgw_capture.py's own imports (ast, _capture_local_modules) and packs every scripts/<name>.py it imports next to it (today: sat_log_to_csv.py); READMEs EN/NL name it. Test tests/test_flash_zip_capture_imports.py calls the real create_distribution_zip() on a temp project with the real scripts/*.py: (a) every local import of otgw_capture.py is in the zip, (b) the zip unpacked into an empty folder runs convert_sat_csv() in an isolated interpreter (-I) on a telnet.log with one SATTICK line and writes sattick.csv. Red on the old build.py: 'flash zip lacks [sat_log_to_csv]' and the field error 'ModuleNotFoundError: No module named sat_log_to_csv'; 3/3 green on the fix. test_flash_zip_modes, test_build, test_capture_tool (one load-flaky run, then OK) green; evaluate --quick 0 FAIL. AC1 (listing of a FRESH build zip) and AC3 (a real capture run from the new zip) are left for the release build / a tester run: not verified here, no build slot.

2026-11-19 AC1 (comms 92, release build of 0f9d85af2 for v2.0.0-beta.392, build.bat EXIT=0, 'Build completed successfully!'): zipfile listing of the FRESH build/OTGW-firmware-esp32-combo-2.0.0-beta.392+0f9d85a-flash.zip (15 files, 15:48:17) shows capture-mqtt-debug.bat, capture-mqtt-debug.sh, capture-mqtt-debug-macos.sh, otgw_capture.py and sat_log_to_csv.py; zip sha256 ee58d31e861c08b198d4bb2ed4e471cd6387c92c9bb40604c82b25b7abf7f48b. AC3 (a real capture run from the new zip) still open: needs a tester run on beta.392.

2026-11-19 AC3 closed (coordinator's call, no waiver: the defect was the missing import). Device side (otgw-bench-tester, relayed by the coordinator): two capture runs from the unpacked 2.0.0-beta.401+47d08b1 flash zip, on .61 and on .63: sat_log_to_csv is imported and runs, satboot.csv (113 B) and satmode.csv written, the 'SCRIPT / CAPTURE ERRORS (script.error.log)' section empty (no ModuleNotFoundError). No sattick.csv there because telnet.log held 0 SATTICK lines: SAT does not tick without OT traffic on the bench and SAT simulation was not approved; convert_sat_csv skips empty record kinds by design. Host side (otgw-dev): the same zip's own otgw_capture.convert_logs_to_csv, run from the unpacked zip on indigo_light's real hour-14 telnet.log (SAT active), wrote sattick.csv (1528 B), errors [].
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
build.py packs every scripts/*.py that otgw_capture.py imports (today sat_log_to_csv.py) into the flash zip, so the capture tool's SAT CSV conversion no longer fails with ModuleNotFoundError. AC1: zipfile listing of the fresh beta.392 build shows otgw_capture.py and sat_log_to_csv.py. AC2: tests/test_flash_zip_capture_imports.py fails on the old build.py and passes on the fix. AC3: device runs by the bench-tester from the unpacked 401 zip on .61 and .63: the module imports and runs (satboot.csv 113 B + satmode.csv), error section empty; no sattick.csv on the bench because there were 0 SATTICK lines (SAT inactive without OT traffic, simulation not approved); and the host run of the same zip's conversion on a real SAT telnet log (indigo hour 14) wrote sattick.csv (1528 B) without errors. Not shown: sattick.csv from one single capture run on a unit with SAT ticking.
<!-- SECTION:FINAL_SUMMARY:END -->
