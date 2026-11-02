---
id: TASK-1556
title: Publish v2.0.0-beta.7 public beta (code = beta.6, delta since v2.0.0-beta.1)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T05:24:00Z'
updated_date: '2026-11-02T05:24:00Z'
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
- [ ] #1 Release notes docs/releases/RELEASE_NOTES_2.0.0-beta.7.md with the delta since beta.1, every claim checked
- [ ] #2 CHANGELOG rollover and bump to beta.7 in one commit; evaluate.py --quick 0 failed
- [ ] #3 esp32-combo build, OTA bench check on .63 (settings unchanged, no reboot, no crashlog)
- [ ] #4 Tag v2.0.0-beta.7, GitHub prerelease --latest=false with assets and SHA256SUMS, download hash verified
- [ ] #5 #alpha-testing zip post and #beta-testing announcement (shown to Robert first)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Checkpoint 1 (2026-11-02, Robert via AskUserQuestion): go for the public beta; tag v2.0.0-beta.7 on a new commit (code identical to beta.6 ca54e4a73) so the tagged tree carries the notes and the CHANGELOG rollover; TASK-1544 (24 h soak, macOS capture run) waived again, stated in the notes. Release prepared in wt-1552 (detached at origin/dev) instead of the main dev tree, because the main tree is diverged and carries peer work.
<!-- SECTION:NOTES:END -->
