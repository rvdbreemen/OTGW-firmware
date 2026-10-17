---
id: TASK-1242
title: 'settings.ini is invalid JSON: missing comma after SATpvboostmaxdurationmin'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-17 20:37'
updated_date: '2026-10-17 20:38'
labels:
  - bug
  - settings
dependencies: []
ordinal: 380000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-17 while backing up the Classic-S3 settings (alpha.430): json.load on /settings.ini fails with "Expecting ',' delimiter: line 144". settingStuff.ino:399 writes SATpvboostmaxdurationmin with withComma=false, but the BLE fields were added after it. The firmware reader (parseJsonKVLine) is line-based and ignores commas, so the device loads every setting; the defect only hits anyone who reads the downloaded settings.ini as JSON (backups, tooling). The alpha.427 backup has the same defect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 json.load of /settings.ini fails on the old firmware and passes on the fix (bench, same settings)
- [ ] #2 Every setting survives the round trip: settings via REST before and after are equal
- [ ] #3 Exactly one writeJson*KV call per settings writer passes withComma=false, and it is the last one
<!-- AC:END -->
