---
id: TASK-1745
title: Public beta v2.0.0-beta.158 (08:00 slot, over beta.145-158)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T05:43:00Z'
updated_date: '2026-11-09T06:28:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 849000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
08:00 public beta per Robert's 12-hour slot rule (/beta-release-2x). Candidate beta.158 (8a4b25d60): 6 h 21 min on bench .61 (OT-Direct, no PSRAM) without restart, MQTT connected, no crashlog. Release commit is docs-only on origin/dev 94406bafc (src identical to 8a4b25d60). Bundles TASK-1719, 1717, 1730, 1693, 1716, 1689, 1731, 1732, 1735, 1737, 1736, 1738, 1741 (R1), 1734.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG rollover and README at the tagged commit
- [x] #2 Build esp32-combo from the tagged commit, SUCCESS lines and fresh artifacts
- [x] #3 Bench evidence: candidate src identical and running without restart on the bench unit
- [x] #4 GitHub prerelease published with --latest=false, assets and SHA256 match
- [ ] #5 #alpha-testing zip post and one #beta-testing announcement
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.158 public prerelease, 08:00 slot, published 2026-11-09 07:27 after Robert invoked /beta-release-2x in session 4c (the publish step had been refused by the permission check until then). AC1: release commit 469f3339d (docs-only: docs/releases/RELEASE_NOTES_2.0.0-beta.158.md over beta.145-158, CHANGELOG rolled over to [2.0.0-beta.158] with the missing TASK-1693/1689/1732/1736/1741/1734/1731 entries, README points at the beta.158 notes); notes lint: no TODO/TBD/em dash, no relative links; evaluate --quick 0 FAIL. AC2: build.bat esp32-combo from 469f3339d, exit 0, artifacts 06:50-07:00 named beta.158+469f333, version string 2.0.0-beta.158+469f333 in the .ino.bin, 469f333 in the littlefs, flash zip 14 files incl. flash and capture tools. The build log came out empty (buffered), so no SUCCESS line was captured. AC3: src identical to 8a4b25d60 (git diff: 0 lines); bench .61 ran beta.158+8a4b25d from the 00:18 flash: uptime 6 h 21 min, bootcount 173 = flash + 2, no crashlog, MQTT connected, freeheap ~71 KB, maxfreeblock 31732 flat (coordinator slot check 06:43 agrees). The release build itself was not flashed (coordinator: .61 is the candidate, .63 off-limits); no Classic S3 Mini soak on this build. AC4: tag v2.0.0-beta.158 -> 469f3339d; release isDraft false, isPrerelease true, latest stays v1.7.9; 7 assets with sizes equal to build/; downloaded flash zip SHA256 44690dfe...73ba equals SHA256SUMS. AC5 (open, not ticked): the #alpha-testing zip post was refused by discord-post-build.sh ('prerelease 2.0.0-beta.158 was already published', the one-tag-per-published-build rule of 2026-10-31); not worked around. The #beta-testing announcement is posted by the comms agent otgw-firmware-97 (discord-mcp down in this session). Deviation from the skill, recorded: separate release worktree instead of the main dev tree (dirty peer files), docs-only release commit without a bump (coordinator's call, to keep the .61 soak evidence and avoid colliding with 80's beta.159). Robert asked to invoke the beta-release-2x workflow next time.
<!-- SECTION:FINAL_SUMMARY:END -->
