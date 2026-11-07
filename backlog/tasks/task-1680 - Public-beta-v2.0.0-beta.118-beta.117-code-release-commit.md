---
id: TASK-1680
title: Public beta v2.0.0-beta.118 (beta.117 code + release commit)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T04:17:00Z'
updated_date: '2026-11-07T09:05:00Z'
labels:
  - release
dependencies: []
priority: high
ordinal: 802000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-07 05:1x: publish a public beta after a 1 h soak. Candidate beta.117 = origin/dev c62500ad9+ plus TASK-1678. Soak on .63 (Classic + S3 Mini; 6b stopped its sampler and freed it at 05:2x). Flow: .claude/skills/beta-release-2x/SKILL.md.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 beta.117 ran 1 h on .63 with MQTT connected: bootcount = flash + 2, no crashlog, heap not trending down
- [x] #2 Release commit (notes, CHANGELOG rollover, README) with evaluate.py --quick 0 failed; esp32-combo build with 2x 'Successfully created ESP32S3 image'
- [x] #3 Bench OTA of the release build on .63: settings unchanged, new fwversion, PIC detected, bootcount stable after 5 min
- [x] #4 Tag pushed, GitHub prerelease published with --latest=false, assets match SHA256SUMS, latest release stays 1.x
- [x] #5 Zip posted in #alpha-testing, announcement in #beta-testing read back intact, ledger rows beta.110-118 added
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 09:30: soak passed on .63 (Classic + S3 Mini): beta.117+2ddef9e, uptime 3h43, bootcount 151 unchanged since 05:44, MQTT connected, crashlog empty, maxfreeblock flat 31732, freeheap live (55488 then 55192). The 08:00 slot was missed (the wake-up watcher was killed by the memory reaper); Robert's 09:15 go stands, release runs now. Bump beta.117 to beta.118 (bin/bump-prerelease.sh, 55 banner files, version strings only). evaluate.py --quick: 92 checks, 85 passed, 0 failed, 7 info.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: .63 (Classic + S3 Mini) on beta.117+2ddef9e from 05:43:14, read at 09:27: uptime 3h43, bootcount 151 (flash + 2, unchanged since 05:44), MQTT connected, crashlog available=false, maxfreeblock flat 31732, freeheap live (55488 then 55192). AC2: release commit 493b226dc (bump beta.117 to beta.118 by bin/bump-prerelease.sh, 55 banner files, version strings only; notes, CHANGELOG rollover, README); evaluate.py --quick 92 checks, 0 failed (scratchpad t1678/eval-118.txt); build-118.log: 2x 'Successfully created ESP32S3 image', 'Build completed successfully', artifacts beta.118+493b226 at 09:55-09:56. AC3: scripts/ota_flash.py to .63 exit 0, app and filesystem confirmed 2.0.0-beta.118+493b226, 'settings: 177 keys, changed: none'; at 10:02:39 (5 min uptime) bootcount 153 (151 + 2), PIC 6.8, MQTT connected, crashlog empty, maxfreeblock 31732. AC4: origin/dev 2ddef9ec0..493b226dc; tag v2.0.0-beta.118 -> 493b226dc; draft asset sizes equal build/ (zip 5962503, ino 2123984, littlefs 786432, merged-full 4128768, elf 25891048, safeboot 931648, SHA256SUMS); published isDraft=false isPrerelease=true; releases/latest stays v1.7.9; downloaded zip sha256 390107df... equals SHA256SUMS. AC5: #alpha-testing zip post 1568551619883434034 (http 200); #beta-testing announcement 1568551684404281384, read back intact; ledger rows beta.110 to beta.118 appended in the main dev tree (110-116 had no post recorded in the ledger and are covered by the 117 and 118 zips). Slot note: planned 08:00 and then 09:15, published 10:04 because the wake-up watcher was killed by the memory reaper.
<!-- SECTION:FINAL_SUMMARY:END -->
