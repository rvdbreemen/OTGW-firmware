---
id: TASK-3090
title: >-
  Beta release script scripts/beta_publish.py: verify soaked sha256, tag, draft,
  publish, re-download check
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-25T21:21:00Z'
updated_date: '2026-11-26T12:14:00Z'
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

2026-11-26 mechanics dry run on the REAL GitHub (beta.431 build from leftovers, notes of beta.432, own ledger, not counted for the gate): steps 1-8 OK, draft id 458697005 created with --target and a throw-away tag name, 7 assets, sizes equal, 177462-char notes shortened to 107866, draft deleted; afterwards gh release list unchanged, 0 drafts, 0 dryrun tags on origin or local. The real notes file exposed a bug the fake data hid: several 'What changed since' sections and the heading is 'SAT is opt-in'; fixed (cut at the first What-changed section, match '## SAT is '), test added (Body: real note layout), 35 tests.

2026-11-26 otgw-webui dry run #1 on R1 (beta.438, release commit 14539282e): PASS exit 0, steps 1-8 ok, nothing published; finding: the script refused the build dir because two flash zips were in it (+1453928 and the stale +bfae2b8), so I ran on a copy with only the +1453928 files; after the run gh release list showed 0 drafts, no dryrun tags locally or on origin, no v2.0.0-beta.438 tag. R2 preparation (coordinator ask, beta.451 = build 628de05ea, release commit a docs-only commit on top, assets keep githash 628de05): scripts/beta_publish.py gets --build-commit X (the zip githash must be X; the release commit must be X or a descendant with an empty diff under src/, libraries/, scripts/, build.py, platformio.ini; without the flag the normal check is unchanged) and --require-text TEXT (repeatable: the release body must contain it, so the SAT-on check numbers cannot be cut by the shortener) plus a guard that the shortened body keeps the notes' disclaimer block. tests/test_beta_publish.py: +12 tests (BuildCommit 7, RequireText 5), full file 47 OK before the last mutation round and 19 (BuildCommit, RequireText, DryRun) OK after; 4 mutants killed (paths narrowed, githash check dropped, require-text ignored, disclaimer check dropped). Commit not pushed yet (push slot to ask).
<!-- SECTION:NOTES:END -->
