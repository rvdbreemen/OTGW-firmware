---
id: TASK-1587
title: Public beta v2.0.0-beta.38 (2026-11-03 morning slot)
status: Done
assignee: []
created_date: '2026-11-03T07:55:00Z'
updated_date: '2026-11-03T07:56:00Z'
labels:
  - release
dependencies: []
priority: high
ordinal: 732000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release record for v2.0.0-beta.38 (bundles beta.12-38 since the last public beta.11).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Maintainer go recorded
- [x] #2 1 h bench soak with MQTT connected on .63, no restart, no crash
- [x] #3 Tag, GitHub prerelease with --latest=false, sha256 verified, #beta-testing announcement read back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Go: Robert 2026-11-03 'Zorg dat de nieuwste beta nog voor 8u getest wordt. Zodat we die kunnen uitbrengen', 'Bericht oke. Klaar voor versturen', soak rule 1 h (same morning), memory-pressure decision 'Restart broker, release 08:20'. Build 2.0.0-beta.38+429fd8c (esp32-combo, 4 images, build.log 'Build completed successfully'), OTA on .63 07:02 (settings 175 keys unchanged). Soak: 07:02-08:51 no restart (bootcount 76 = 72 + 2 flashes x 2), no crashlog, cmd_dropped 0 (TASK-1580 fix live), heap 56.8 KB steady, MQTT connected 07:02-07:15 and 07:26-08:51 (broker was killed by Claude Code memory reaper 07:15, restarted on maintainer decision). Tag v2.0.0-beta.38 on 429fd8c58; GitHub prerelease published, latest stays v1.7.8; 7 assets sizes equal build/; downloaded zip sha256 4e7e7b26ee58... = SHA256SUMS; zip holds flash_otgw.bat/.sh, capture scripts, READMEs. Alpha zip msg 1567056401078161429; #beta-testing msg 1567084655042367590 read back intact.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.38 published as public prerelease after a 1 h MQTT soak on the Classic + S3 Mini bench unit; tag on 429fd8c, assets and sha256 verified, latest stays 1.7.8, announcement posted and read back.
<!-- SECTION:FINAL_SUMMARY:END -->
