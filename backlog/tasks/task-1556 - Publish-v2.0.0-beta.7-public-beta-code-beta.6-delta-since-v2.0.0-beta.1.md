---
id: TASK-1556
title: Publish v2.0.0-beta.7 public beta (code = beta.6, delta since v2.0.0-beta.1)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T05:24:00Z'
updated_date: '2026-11-02T05:47:00Z'
labels:
  - release
dependencies: []
priority: high
ordinal: 709000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: publish the current beta in the public beta channel with the delta to the previous public release (v2.0.0-beta.1). Flow: /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes docs/releases/RELEASE_NOTES_2.0.0-beta.7.md with the delta since beta.1, every claim checked
- [x] #2 CHANGELOG rollover and bump to beta.7 in one commit; evaluate.py --quick 0 failed
- [x] #3 esp32-combo build, OTA bench check on .63 (settings unchanged, no reboot, no crashlog)
- [x] #4 Tag v2.0.0-beta.7, GitHub prerelease --latest=false with assets and SHA256SUMS, download hash verified
- [x] #5 #alpha-testing zip post and #beta-testing announcement (shown to Robert first)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Checkpoint 1 (2026-11-02, Robert via AskUserQuestion): go for the public beta; tag v2.0.0-beta.7 on a new commit (code identical to beta.6 ca54e4a73) so the tagged tree carries the notes and the CHANGELOG rollover; TASK-1544 (24 h soak, macOS capture run) waived again, stated in the notes. Release prepared in wt-1552 (detached at origin/dev) instead of the main dev tree, because the main tree is diverged and carries peer work.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.7 published 2026-11-02 as GitHub prerelease (--latest=false, v1.7.8 stays Latest): https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.7, tag on cae98c57b (code = beta.6 ca54e4a73). AC1: notes docs/releases/RELEASE_NOTES_2.0.0-beta.7.md, delta since beta.1 (TASK-1547, 1480, 1552, 1315, 1542, 1536), no dashes/TODO/relative links/OTGW32. AC2: CHANGELOG rolled over to [2.0.0-beta.7] (+ missing 1552/1536 entries), bump beta.7, evaluate --quick 0 failed. AC3: build esp32-combo 2x 'Successfully created ESP32S3 image' + 'Build completed successfully'; OTA app+fs to .63, settings 175 keys changed none, bootcount 56 unchanged after 5 min, crashlog only the old 02:17 TWDT test (elf ba693715). AC4: 7 assets, sizes = build/, downloaded zip sha256 f319836e...76474 = SHA256SUMS. AC5: #alpha-testing zip post msg 1566689143797448835; #beta-testing announcement shown to Robert, sent msg 1566689444755406858, read back intact. Waiver: TASK-1544 (24 h soak, macOS) by Robert, stated in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
