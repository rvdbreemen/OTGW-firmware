---
id: TASK-1067
title: Close the beta/stable release-asset gap and harden the beta-prerelease skill
status: In Progress
assignee:
  - '@claude'
created_date: '2026-08-08 15:09'
updated_date: '2026-08-08 15:14'
labels:
  - tooling
  - ci
dependencies: []
priority: medium
ordinal: 177000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three beta runs in one session (beta.2, beta.3, beta.4) plus the TASK-936 stable fix exposed four gaps. (1) The capture scripts and the RELEASE_ASSETS.md guide were added to release-assets.yml (stable) but NOT to beta-prerelease.yml, which is a separate self-contained workflow per Trap 3. That is backwards: betas are where bug reports come from, and the beta.4 announcement asked testers to report findings while shipping them no capture script and no instructions for producing one. (2) The skill's Phase 3 staleness check greps for keywords and gave a false pass on beta.4: a match on the word 'ventilation' inside an unrelated older bullet masked the fact that both new fixes were absent from the CHANGELOG. A TASK-NNN set comparison is mechanical and has no false positives. (3) Phase 6 forbids git add -A but gives no safe way to enumerate the ~27 files the bump rewrites; the audit-then-stage pattern that worked three times is undocumented. (4) Phase 8's expected-asset list needs to match whatever (1) attaches.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 beta-prerelease.yml attaches the capture scripts and a generated RELEASE_ASSETS.md, and includes them in the flash bundle
- [x] #2 Phase 3 of the skill uses a TASK-NNN set comparison between commits since LATEST_PUBLIC and the CHANGELOG [Unreleased] section, replacing the keyword grep
- [x] #3 Phase 6 of the skill documents the audit-then-stage recipe: prove every src/ change is banner-or-version-only, then stage by explicit path
- [x] #4 Phase 8 of the skill lists the full expected asset set matching what beta-prerelease.yml now attaches
- [x] #5 beta-prerelease.yml YAML parses and every run block passes bash -n
- [x] #6 The RELEASE_ASSETS.md generator is executed, not assumed, and its output inspected
<!-- AC:END -->
