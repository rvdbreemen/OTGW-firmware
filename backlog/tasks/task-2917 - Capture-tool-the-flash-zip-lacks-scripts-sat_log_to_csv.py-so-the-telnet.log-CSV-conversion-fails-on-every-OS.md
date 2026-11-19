---
id: TASK-2917
title: >-
  Capture tool: the flash zip lacks scripts/sat_log_to_csv.py, so the telnet.log
  CSV conversion fails on every OS
status: In Progress
assignee:
  - '@5e'
created_date: '2026-11-19T13:57:00Z'
updated_date: '2026-11-19T14:21:00Z'
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
- [ ] #1 build.py packs scripts/sat_log_to_csv.py next to otgw_capture.py in the flash zip (zipfile listing of a fresh build shows it)
- [x] #2 A test fails on the old build.py and passes on the fix (the packed file list contains every module otgw_capture.py imports from scripts/)
- [ ] #3 A capture run from the new zip writes sattick.csv without the ModuleNotFoundError line in the transcript
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 5e: build.py now reads otgw_capture.py's own imports (ast, _capture_local_modules) and packs every scripts/<name>.py it imports next to it (today: sat_log_to_csv.py); READMEs EN/NL name it. Test tests/test_flash_zip_capture_imports.py calls the real create_distribution_zip() on a temp project with the real scripts/*.py: (a) every local import of otgw_capture.py is in the zip, (b) the zip unpacked into an empty folder runs convert_sat_csv() in an isolated interpreter (-I) on a telnet.log with one SATTICK line and writes sattick.csv. Red on the old build.py: 'flash zip lacks [sat_log_to_csv]' and the field error 'ModuleNotFoundError: No module named sat_log_to_csv'; 3/3 green on the fix. test_flash_zip_modes, test_build, test_capture_tool (one load-flaky run, then OK) green; evaluate --quick 0 FAIL. AC1 (listing of a FRESH build zip) and AC3 (a real capture run from the new zip) are left for the release build / a tester run: not verified here, no build slot.
<!-- SECTION:NOTES:END -->
