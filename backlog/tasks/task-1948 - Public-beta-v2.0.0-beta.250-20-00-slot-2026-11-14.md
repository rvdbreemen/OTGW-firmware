---
id: TASK-1948
title: Public beta v2.0.0-beta.250 (20:00 slot 2026-11-14)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-14T20:20:00Z'
updated_date: '2026-11-14T20:59:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 995000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert ran /beta-release-2x at 21:1x. Candidate beta.250+baf84d9, 1 h 02 soak on .63 (bootcount 131, MQTT up, no crashlog). Bundles beta.238-250. Embargo B: TASK-1934 described neutrally.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG rollover, README for beta.250, no em dashes, claims checked
- [x] #2 Build from the release commit, 2 image lines, bench check on .63 (settings diff none, bootcount stable 5 min, no crashlog)
- [x] #3 Tag v2.0.0-beta.250 on the release commit, prerelease --latest=false, assets + SHA256SUMS match
- [x] #4 #alpha-testing zip post + #beta-testing announcement
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 0-2: freeze sent 21:16; main tree synced to origin/dev a5a650c50 (local .wolf/task-1808 edits saved as a patch in the coordinator scratchpad); ledger covers 238-248+250 (249 never on origin). Narrative: RELEASE_NOTES_2.0.0-beta.250.md (all updates since 237, task-ID cross-check vs git log), CHANGELOG [Unreleased] filled and rolled to [2.0.0-beta.250], README. Embargo B: TASK-1934 one neutral line; TASK-1936 wording neutralised. No version bump (beta.7/237 model: version.h already beta.250). evaluate --quick: 0 failed, 100%.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: RELEASE_NOTES_2.0.0-beta.250.md (every change 238-250, task-ID cross-check vs git log; TASK-1934 one neutral line per embargo B), CHANGELOG rolled to [2.0.0-beta.250], README; grep for em dash/TODO/relative links clean; evaluate --quick 0 failed. AC2: build.bat on aed38bfae: 2x 'Successfully created ESP32S3 image', 'Build completed successfully!', fresh artifacts 21:47; 09 bench on .63: ota exit 0, 150 keys changed none, fwversion 2.0.0-beta.250+aed38bf, PIC 6.8, bootcount 131->133->133 at t+5:31, no crashlog, MQTT up, settings diff []. AC3: dev pushed a5a650c50..aed38bfae, annotated tag v2.0.0-beta.250 on aed38bfae, draft then published prerelease --latest=false, 7 assets sizes equal build/, latest stays v1.7.9, downloaded zip sha256 966ce852... = SHA256SUMS. Body trimmed to 95 KB (GitHub limit 125000) with a link to the full notes. AC4: #beta-testing 1571268250338332822 (read back intact); #alpha-testing note with release link; zip post refused by discord-post-build.sh (prerelease number already posted as baf84d9, 1571207444875444356). Robert gave standing permission for public betas during this run.
<!-- SECTION:FINAL_SUMMARY:END -->
