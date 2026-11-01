---
id: TASK-1545
title: >-
  Release and beta-release flows for the 2.x firmware (skills, modelled on the
  1.x /release and /beta-prerelease)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T15:55:00Z'
updated_date: '2026-11-01T16:28:00Z'
labels:
  - release
  - tooling
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 701000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 1.x skills do not fit dev: /beta-prerelease targets otgw-1.x.x and a CI workflow that dev lacks; /release merges dev into main, and main is not an ancestor of dev (1840 commits apart). v2.0.0-beta.1 was released by hand on 2026-11-01 (TASK-1504); codify that flow and a stable 2.x flow.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A 2.x beta-release skill under .claude/skills/ that encodes the manual flow used for v2.0.0-beta.1 (bump via autoinc-semver --prerelease, CHANGELOG rollover before the tag, combo build, OTA bench, annotated tag, draft-first gh release --prerelease --latest=false, asset + SHA256 checks, #alpha-testing post, #beta-testing announcement checkpoint)
- [x] #2 A 2.x stable release skill under .claude/skills/ with the main-branch strategy Robert chose
- [x] #3 CLAUDE.md names both skills; the 1.x skills state they are 1.x-only
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Robert 2026-11-01 (AskUserQuestion): stable 2.x releases come from a separate release/2.x branch; main stays 1.x. Checked: main is not an ancestor of dev (git merge-base --is-ancestor fails; 1840 commits on main not in dev). Found while writing: _VERSION_PRERELEASE must be commented out for a stable build, not set to 'none', because #if defined(_VERSION_PRERELEASE) guards debug-only code in networkStuff.ino (205-414) and OTGW-firmware.ino:495; the pre-commit hook then needs the documented one-commit bypass.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Two new skills: .claude/skills/beta-release-2x/SKILL.md (2.x beta from dev, codifies the v2.0.0-beta.1 run of 2026-11-01: autoinc-semver --prerelease, CHANGELOG rollover before the tag, esp32-combo build, OTA bench, annotated tag, draft-first gh release --prerelease --latest=false, SHA256 download check, #alpha-testing zip post from a worktree at the tag, #beta-testing announcement checkpoint) and .claude/skills/release-2x/SKILL.md (stable 2.x on release/2.x per Robert's choice; 3 checkpoints incl. the GitHub Latest decision; production commit comments out _VERSION_PRERELEASE; docs back to dev by cherry-pick only). The 1.x /release and /beta-prerelease state they are 1.x-only; CLAUDE.md gets a Release flows table and the release/2.x push rule. Evidence: AC1 every command in the beta skill ran today for v2.0.0-beta.1 (TASK-1504 notes); AC2 dry-run of the production step on a copy of version.h: '//#define _VERSION_PRERELEASE beta.1', _SEMVER_FULL '2.0.0+<hash>', banner 'v2.0.0'; tests/test_adr_governance.py OK. Not verified: a real stable run (first use at 2.0.0), the /update-docs step on 2.x.
<!-- SECTION:FINAL_SUMMARY:END -->
