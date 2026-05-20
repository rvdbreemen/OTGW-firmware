---
id: TASK-639
title: Inline 'since last public release' digest in beta GitHub release body
status: To Do
assignee: []
created_date: '2026-05-20 15:36'
labels:
  - release
  - docs
  - ci
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The beta-prerelease.yml workflow currently composes a thin release body that only links README.md, CHANGELOG.md, and (if present) RELEASE_NOTES_<base>-beta.md. Field testers reading the release page on mobile want a narrative summary of what changed since the last shipping version, inline. Curate that summary in a new RELEASE_NOTES_1.6.0-beta.md at repo root, use an HTML-comment sentinel to mark a teaser region, and modify the workflow to inline the teaser between the 'What is in this build' section and the 'Compare to the latest public release' section.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 RELEASE_NOTES_1.6.0-beta.md exists at the repo root with a curated 'What's new since v1.5.0-fix2' narrative, grouped by area (MQTT/HA discovery, Web UI/diagnostics, tooling/build, code hygiene, documentation). No em dashes.
- [ ] #2 The notes file contains an HTML-comment digest-end sentinel (e.g. '<!-- digest:end -->') marking the teaser cutoff. Content above the sentinel is the release-page digest; content below is the long-form detail referenced via the existing 'Read full release notes' link.
- [ ] #3 .github/workflows/beta-prerelease.yml inlines the teaser content (lines 1..sentinel, sentinel line stripped) under a new '## What's new since the last public release' heading inside the composed release body, placed between 'What is in this build' and 'Compare to the latest public release'.
- [ ] #4 When no notes file is found at the tagged commit, the workflow falls back to the previous behaviour (no inline summary, no 'Read full release notes' link) without erroring.
- [ ] #5 When the notes file exists but has no digest-end sentinel, the workflow inlines the entire file (best-effort behaviour, documented in the workflow comments).
- [ ] #6 python build.py --firmware exits 0 and python evaluate.py --quick shows no new failures on the commit that introduces the change.
- [ ] #7 Changes commit to claude/beta-release-prep-qUipc and a draft PR is opened against dev.
<!-- AC:END -->
