---
id: TASK-1934
title: Normalise file paths before the access check in the file handler
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T12:46:00Z'
updated_date: '2026-11-14T13:45:00Z'
labels:
  - bug
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 990000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
handleFile() (FSexplorer.ino) takes access decisions and reads files on the request path as received. It should first bring the path to canonical form (collapse repeated '/', drop '.', resolve '..', refuse a path above the root) and use that one path for the settings.ini/settings.tmp access check, the i18n route, the exists check and the read. Without an admin password nothing changes (maintainer: trusted-network model). The 1.x line gets the same change in its own task. Background details are kept outside the repository for now (coordinator).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test that compiles the real handleFile(): other spellings of the settings.ini path (dot segments, doubled slashes) reach the access check; red before the fix
- [x] #2 Fix: one canonical path for the access check, the i18n route, the exists check and the read; red-then-green
- [x] #3 The other path-taking routes are checked; the 1.x line has its own task
- [x] #4 Bench: with an admin password set, every spelling of the settings.ini path needs the password
- [x] #5 Bench: without an admin password nothing changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (09): fix in FSexplorer.ino fsCanonicalPath(), used first in handleFile(). Host test tests/test_web_file_scheduling.py settings_guard_sees_the_canonical_path (the fake LittleFS resolves '.' and '..' like lfs_dir_find): red on the code before the fix (--old-rev origin/dev, 15 failures), green on the fix; new mutant raw-path-guard killed; the five older mutants still killed. evaluate.py --quick 0 failed. Endpoint check (settings API, /ReBoot, /ResetWireless, /update, FS delete, /upload): all use checkHttpAuth or authenticate and match routes, not raw file paths. Bench on the device follows with build beta.245.

2026-11-14 (09) bench .63 on 2.0.0-beta.245+662a2a8 (pushed 662a2a859) with a temporary admin password (removed afterwards, settings diff [] API and raw ini): 24 spellings of the settings.ini/settings.tmp path, with FSexplorer present and after removing it: none returned the file without the password (401, or 404 for paths above the root and undecodable spellings); with the password the file is served. Without a password the file stays readable as before (/settings.ini and dot-segment spellings 200). Evidence: 09 scratchpad b245/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-2: host test compiles the real handleFile(); red before the fix, green after; mutant raw-path-guard killed. AC3: the other path-taking routes use the access check on route names; 1.x has its own task. AC4-5: bench .63 beta.245 with and without an admin password as described in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
