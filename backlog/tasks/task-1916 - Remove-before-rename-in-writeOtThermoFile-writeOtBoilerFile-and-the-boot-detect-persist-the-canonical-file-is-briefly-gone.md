---
id: TASK-1916
title: >-
  Remove-before-rename in writeOtThermoFile/writeOtBoilerFile and the
  boot-detect persist: the canonical file is briefly gone
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T00:30:00Z'
updated_date: '2026-11-14T22:39:00Z'
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
- [x] #1 writeOtThermoFile, writeOtBoilerFile and the boot-detect persist rename over the canonical file without removing it first, and check the rename
- [x] #2 Host or static test red on the old code, green on the fix; test_board_detect_persist_gate stays green
- [ ] #3 Bench: OT support files and the boot-detect verdict survive save + reboot
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Fix (ba 2026-11-14, lane wt-ba-1916 on top of the beta.264 stack): writeOtThermoFile() and writeOtBoilerFile() (OTGW-Core.ino) and appendBootDetectLog() (OTGW-firmware.ino) no longer remove the live file; they take the written size from position() (as writeSettings, settingStuff.ino:520; size() of an open file may lag the stdio buffer) and commit through fsCommitTempFile() (TASK-1913), which checks the size on flash and the rename result and keeps the old file on a short write. Correction to the description: OTGW-firmware.ino:211 is appendBootDetectLog(), the /bootdetect.log diagnostic history, not the ADR-201 board verdict (that is not written through this temp file); the ADR-201 gate is unaffected. AC1/AC2: tests/test_fs_rename_without_remove.py (static, per function body): old 6/6 FAIL (OTGW_FSREN_REV=HEAD), fix OK; test_board_detect_persist_gate OK; test_ot_thermo_file_v3, test_fs_update_keep, test_dallas_labels_ram_copy OK; evaluate --quick 0 FAIL. Not verified on hardware: that position() equals the bytes on flash for these writers (AC3 bench: OT support files and /bootdetect.log survive save + reboot, and a fresh profile still persists).
<!-- SECTION:NOTES:END -->
