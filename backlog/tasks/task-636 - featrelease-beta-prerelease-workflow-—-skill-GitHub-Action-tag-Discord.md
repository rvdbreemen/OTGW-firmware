---
id: TASK-636
title: >-
  feat(release): beta-prerelease workflow — skill + GitHub Action + tag +
  Discord
status: To Do
assignee: []
created_date: '2026-05-20 09:43'
labels:
  - release
  - tooling
  - ci
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Develop a complete beta-prerelease workflow that parallels the existing /release skill but for the lightweight beta cycle. Currently bin/bump-prerelease.sh handles the version mechanics and CLAUDE.md documents the bump policy in prose, but there is no consolidated, repeatable flow for shipping a beta build to the field-testers in Discord #beta-testing. This task delivers (1) a .claude/skills/beta-prerelease/SKILL.md slash-command that orchestrates the local cycle end-to-end, (2) a .github/workflows/beta-prerelease.yml GitHub Action that fires on a v*-beta.* tag push, builds firmware + filesystem binaries, and publishes a GitHub prerelease so the existing release-assets.yml workflow chains in to attach SHA256SUMS / flash scripts / bundle, and (3) a documented Discord-announcement step so testers get a one-line ping in #beta-testing with the new version string and a one-paragraph change summary.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Skill file .claude/skills/beta-prerelease/SKILL.md exists with name+description frontmatter and disable-model-invocation:true (matching the /release skill convention)
- [ ] #2 Skill documents the full local flow: clean-state check on dev, run bin/bump-prerelease.sh, build.py --firmware green, evaluate.py --quick green, stage firmware change + version.h + data/version.hash, commit with conventional message, push origin/dev, create+push tag vSEMVER_CORE-PRERELEASE (e.g. v1.6.0-beta.4), post to Discord #beta-testing
- [ ] #3 Skill writing-style rules are explicit: no em dashes, English-only release notes, no emojis (matching the /release skill style block)
- [ ] #4 GitHub Action .github/workflows/beta-prerelease.yml triggers on push of tags matching v*-beta.* and supports workflow_dispatch for manual re-run
- [ ] #5 Action builds firmware (.ino.bin) and filesystem (.littlefs.bin) binaries using the same build.py invocation as a local build
- [ ] #6 Action creates a GitHub release marked prerelease:true, with the tag name as title and a body containing the version string and a link to the dev branch diff since the previous prerelease tag
- [ ] #7 Action uploads .ino.bin and .littlefs.bin as release assets so the existing release-assets.yml workflow (triggered on release:published) chains in to attach SHA256SUMS, flash_otgw.sh/.bat, and the flash-bundle zip
- [ ] #8 Documentation lives in the skill itself (no separate docs/guides/ file) to keep the source of truth singular, matching the /release skill pattern
- [ ] #9 Skill explicitly differentiates itself from /release: beta does not merge to main, does not edit README, does not bump _SEMVER_CORE (only _VERSION_PRERELEASE), and is repeatable many times within one minor cycle
- [ ] #10 Verification: a dry-run section in the skill explains how to test the workflow without actually publishing (e.g. push a vX.Y.Z-beta.test tag to a throwaway branch and confirm the Action fires)
- [ ] #11 python build.py --firmware exits 0 after the changes (no firmware code is modified, but the gate still applies)
- [ ] #12 python evaluate.py --quick shows no new failures
- [ ] #13 Final-summary section of the task contains a PR-style summary suitable for the GitHub PR body
<!-- AC:END -->
