---
id: TASK-1916
title: >-
  Remove-before-rename in writeOtThermoFile/writeOtBoilerFile and the
  boot-detect persist: the canonical file is briefly gone
status: To Do
assignee: []
created_date: '2026-11-14T00:30:00Z'
updated_date: '2026-11-14T00:31:00Z'
labels:
  - reliability
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 982000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during TASK-1913 (95 confirmed). OTGW-Core.ino writeOtThermoFile() (LittleFS.remove(canonicalPath) at ~7542 then rename at 7543) and writeOtBoilerFile() (7561-7562), and OTGW-firmware.ino:211-212 (boot-detect persist, BOOTDETECT_TMP -> BOOTDETECT_FILE, ADR-201, gated by tests/test_board_detect_persist_gate.py) remove the canonical file before the rename. lfs_rename replaces an existing destination in one commit (TASK-1913 notes: esp_littlefs vfs_littlefs_rename -> lfs_rename, no unlink), so the remove is unnecessary and opens a window where a reset leaves only the .tmp: the OT support files fall back to the defaults, and the boot-detect verdict is lost (re-probe next boot). Fix: drop the removes, check the rename result, ideally through fsCommitTempFile() (TASK-1913, helperStuff.ino). Keep ADR-201's persist-only-a-positive-verdict rule and its gate green.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 writeOtThermoFile, writeOtBoilerFile and the boot-detect persist rename over the canonical file without removing it first, and check the rename
- [ ] #2 Host or static test red on the old code, green on the fix; test_board_detect_persist_gate stays green
- [ ] #3 Bench: OT support files and the boot-detect verdict survive save + reboot
<!-- AC:END -->
