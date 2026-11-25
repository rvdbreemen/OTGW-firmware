---
id: TASK-2033
title: Public beta v2.0.0-beta.374 (2026-11-18 20:00 slot)
status: Done
assignee: []
created_date: '2026-11-18T17:34:00Z'
updated_date: '2026-11-25T06:42:00Z'
labels:
  - release
dependencies: []
priority: high
ordinal: 1039000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta through /beta-release-2x on the release commit on top of beta.374 (1d124dbc5). Covers beta.361 to beta.374 (beta.366 never landed; beta.375 TASK-2025 not in, its bench OK came after the release commit). Notes docs/releases/RELEASE_NOTES_2.0.0-beta.374.md; CHANGELOG rollover with the TASK-1991/TASK-2007 lines moved out of the released beta.360 section; README links to 374. Timeline (coordinator): release commit ~18:55, build ~19:00-19:10, 1 h soak on .63 19:12-20:12, publish ~20:15 with the full asset check; skip to 08:00 on a soak failure.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit pushed; build of that commit made with build.bat (esp32-combo), artifacts checked
- [ ] #2 1 h soak on .63 passes every item of the decision list (bootcount, no crash, MQTT, SAT, min free heap stated, no unresponsive period, no loop stall, settings unchanged)
- [x] #3 Tag on the release commit, draft prerelease with --latest=false, assets verified (sizes, sha256 of the downloaded zip), published
- [ ] #4 #beta-testing announcement and #alpha-testing note posted and read back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 otgw-docs (95), closed on Robert's DM A (relayed by the coordinator). Checked with gh/git today: release v2.0.0-beta.374 exists, isDraft=false, isPrerelease=true, published 2026-11-18T19:05:09Z, https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.374. The tag points at c97903857 (docs(release): v2.0.0-beta.374 release notes, CHANGELOG rollover and README); the release commit named in the task, 1d124dbc5, is its ancestor. 7 assets (flash zip, ino.bin, littlefs.bin, merged-full.bin, elf, safeboot bin, SHA256SUMS); I downloaded the flash zip and the ino.bin and sha256sum -c against the published SHA256SUMS: both OK. The githash in the asset names (c979038) is the tag commit. AC1 and AC3 ticked on that evidence. AC2 (the 1 h soak list) and AC4 (the #beta-testing and #alpha-testing posts) were NOT re-verified by me: the soak data and the Discord posts were not looked at in this session; left unticked on purpose.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
published, evidence: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.374 (prerelease, published 2026-11-18, tag c97903857, flash zip and ino.bin match SHA256SUMS). Not re-verified by otgw-docs: the 1 h soak (AC2) and the Discord posts (AC4).
<!-- SECTION:FINAL_SUMMARY:END -->
