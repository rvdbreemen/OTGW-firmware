---
id: TASK-3090
title: >-
  Beta release script scripts/beta_publish.py: verify soaked sha256, tag, draft,
  publish, re-download check
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T21:21:00Z'
updated_date: '2026-11-25T21:31:00Z'
labels:
  - tooling
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1159000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release pipeline idea iii (OTGW-field-data/_comms/release_pipeline_design.md). One command from release commit + build dir + soaked SHA256 + soak verdict file to a published beta prerelease: verify build hashes equal the soaked ones, annotated tag on the release commit, SHA256SUMS in scratch, draft prerelease --latest=false, verify sizes and flash zip contents, publish, re-download and compare sha256, print what to post. --dry-run runs up to and including the draft against a throw-away tag and deletes it. No Discord posting. Not for a real release before two dry runs on betas succeeded; never first used at the stable.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 scripts/beta_publish.py refuses on: zip githash not the release commit, em dash in body, latest flag, soaked-hash mismatch, missing or failed soak verdict; builds the shortened body when over 125000 chars
- [x] #2 Tests with a fake gh cover every step and every refusal (red on a mutant, green on the script)
- [ ] #3 --dry-run runs against a throw-away tag name and deletes the draft and tag; two dry runs on real betas succeed before first real use
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
otgw-webui 2026-11-26: scripts/beta_publish.py + tests/test_beta_publish.py (34 tests, real git with a local bare origin, fake gh in tests/fixtures/fake_gh.py): 34 OK in 139 s. Nine mutants of the script (soaked-sha check, em-dash check, githash gate, --latest=false, two-dry-run gate, body shortening, re-download compare, size compare, draft cleanup) each make at least one test fail. Real gh 2.96.0 checked read-only: create has --latest --verify-tag --target --notes-file --draft --prerelease --title, download has --dir --clobber, api releases listing works. NOT verified: a draft against real GitHub (--target with a throw-away tag name) and a published run; AC3 stays open until two dry runs on two real betas.
<!-- SECTION:NOTES:END -->
