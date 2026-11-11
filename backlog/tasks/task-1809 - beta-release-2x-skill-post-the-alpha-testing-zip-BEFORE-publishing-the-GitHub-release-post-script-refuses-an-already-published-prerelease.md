---
id: TASK-1809
title: >-
  beta-release-2x skill: post the #alpha-testing zip BEFORE publishing the
  GitHub release (post script refuses an already-published prerelease)
status: To Do
assignee: []
created_date: '2026-11-11T07:07:00Z'
labels:
  - tooling
  - release
milestone: 2.0.0
dependencies: []
ordinal: 902000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On v2.0.0-beta.174 (TASK-1794) Phase 6.1 ran after Phase 5 as the skill orders it, and scripts/discord-post-build.sh refused: 'prerelease 2.0.0-beta.174 was already published'. Same as beta.158 (memory feedback_public_beta_via_skill). Fix: move the zip post to Phase 4 (after the tag + draft, before publish), or let the script accept a release whose tag commit equals the zip githash.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SKILL.md order changed (or script rule changed) with the reason
- [ ] #2 A dry run shows the post step passes before publish
<!-- AC:END -->
