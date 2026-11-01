---
id: TASK-1388
title: >-
  flash_otgw --migrate: settings.ini restore check fails on a version jump (byte
  compare against the old file)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T11:36:00Z'
updated_date: '2026-11-01T01:36:00Z'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 527000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563871156329775135): migrate from alpha.404 to alpha.500 succeeded and his settings look fine, but the script printed '[ERROR] settings.ini on the unit differs from the saved copy'. flash_otgw.bat:496-499 downloads settings.ini after the restore + reboot and compares it byte for byte (fc /b) with the saved copy. The new firmware reads the old file and writes it back in its own format (new keys, defaults, numeric precision, migrated values), so a version jump always differs. Fix: compare per key: every key of the saved file must be present after the restore with the same value (numbers compared as numbers), new keys allowed; report the keys that differ. Same check in flash_otgw.sh.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A migrate across a version jump with all settings kept reports OK (repro with an old settings.ini, red on HEAD)
- [x] #2 A real loss (a saved key missing or changed) is still reported, naming the keys
- [x] #3 flash_otgw.bat and flash_otgw.sh behave the same
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 tester feedback: needs Robert's OK before implementation (testers have no admin control).

2026-10-30 maintainer decision (Robert, in session otgw-firmware-ea): released for pickup ('vrijgeven'); label needs-maintainer-ok removed. Priority after beta-1 blockers unless it is one.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
flash_otgw.bat and flash_otgw.sh compare settings.ini per key after a --migrate restore instead of byte for byte: every saved key must come back with the same value (numbers compared as numbers, relative 1e-6), new keys are allowed, a loss or change names the keys only (never values, so no passwords in the output). bat: inline Windows PowerShell with PSModulePath cleared (PS 5.1 started from pwsh 7 otherwise misses its modules); sh: awk. Both expose --compare-settings <saved> <after> for tests.
Evidence: the report's case reproduced with synthetic files (rewritten order, 0.35 -> 0.349999994, a new key): old fc /b exit 1 and old cmp -s exit 1 (the old scripts print ERROR); new bat and sh exit 0 with OK. tests/test_flash_migrate_settings_compare.py 5/5 (bat via cmd, sh via Git Bash; a missing key and a changed password are named, the value is not printed). test_flash_zip_modes, test_flash_usb_pid_filter, test_build_safeboot pass.
Not verified: a real USB migrate across a version jump (not run on the bench).
<!-- SECTION:FINAL_SUMMARY:END -->
