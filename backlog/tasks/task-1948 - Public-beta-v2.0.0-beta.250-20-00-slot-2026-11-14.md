---
id: TASK-1948
title: Public beta v2.0.0-beta.250 (20:00 slot 2026-11-14)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-14T20:20:00Z'
updated_date: '2026-11-14T20:36:00Z'
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
- [ ] #1 Release notes, CHANGELOG rollover, README for beta.250, no em dashes, claims checked
- [ ] #2 Build from the release commit, 2 image lines, bench check on .63 (settings diff none, bootcount stable 5 min, no crashlog)
- [ ] #3 Tag v2.0.0-beta.250 on the release commit, prerelease --latest=false, assets + SHA256SUMS match
- [ ] #4 #alpha-testing zip post + #beta-testing announcement
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Phase 0-2: freeze sent 21:16; main tree synced to origin/dev a5a650c50 (local .wolf/task-1808 edits saved as a patch in the coordinator scratchpad); ledger covers 238-248+250 (249 never on origin). Narrative: RELEASE_NOTES_2.0.0-beta.250.md (all updates since 237, task-ID cross-check vs git log), CHANGELOG [Unreleased] filled and rolled to [2.0.0-beta.250], README. Embargo B: TASK-1934 one neutral line; TASK-1936 wording neutralised. No version bump (beta.7/237 model: version.h already beta.250). evaluate --quick: 0 failed, 100%.
<!-- SECTION:NOTES:END -->
