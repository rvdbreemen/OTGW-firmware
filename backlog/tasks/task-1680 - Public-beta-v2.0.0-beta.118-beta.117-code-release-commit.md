---
id: TASK-1680
title: Public beta v2.0.0-beta.118 (beta.117 code + release commit)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-07T04:17:00Z'
updated_date: '2026-11-07T08:29:00Z'
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
- [ ] #1 beta.117 ran 1 h on .63 with MQTT connected: bootcount = flash + 2, no crashlog, heap not trending down
- [ ] #2 Release commit (notes, CHANGELOG rollover, README) with evaluate.py --quick 0 failed; esp32-combo build with 2x 'Successfully created ESP32S3 image'
- [ ] #3 Bench OTA of the release build on .63: settings unchanged, new fwversion, PIC detected, bootcount stable after 5 min
- [ ] #4 Tag pushed, GitHub prerelease published with --latest=false, assets match SHA256SUMS, latest release stays 1.x
- [ ] #5 Zip posted in #alpha-testing, announcement in #beta-testing read back intact, ledger rows beta.110-118 added
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-07 09:30: soak passed on .63 (Classic + S3 Mini): beta.117+2ddef9e, uptime 3h43, bootcount 151 unchanged since 05:44, MQTT connected, crashlog empty, maxfreeblock flat 31732, freeheap live (55488 then 55192). The 08:00 slot was missed (the wake-up watcher was killed by the memory reaper); Robert's 09:15 go stands, release runs now. Bump beta.117 to beta.118 (bin/bump-prerelease.sh, 55 banner files, version strings only). evaluate.py --quick: 92 checks, 85 passed, 0 failed, 7 info.
<!-- SECTION:NOTES:END -->
