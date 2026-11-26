---
id: TASK-3090
title: >-
  Beta release script scripts/beta_publish.py: verify soaked sha256, tag, draft,
  publish, re-download check
status: Done
assignee:
  - '@claude'
created_date: '2026-11-25T21:21:00Z'
updated_date: '2026-11-26T13:18:00Z'
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
- [x] #3 --dry-run runs against a throw-away tag name and deletes the draft and tag; two dry runs on real betas succeed before first real use
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
otgw-webui 2026-11-26: scripts/beta_publish.py + tests/test_beta_publish.py (34 tests, real git with a local bare origin, fake gh in tests/fixtures/fake_gh.py): 34 OK in 139 s. Nine mutants of the script (soaked-sha check, em-dash check, githash gate, --latest=false, two-dry-run gate, body shortening, re-download compare, size compare, draft cleanup) each make at least one test fail. Real gh 2.96.0 checked read-only: create has --latest --verify-tag --target --notes-file --draft --prerelease --title, download has --dir --clobber, api releases listing works. NOT verified: a draft against real GitHub (--target with a throw-away tag name) and a published run; AC3 stays open until two dry runs on two real betas.

2026-11-26 mechanics dry run on the REAL GitHub (beta.431 build from leftovers, notes of beta.432, own ledger, not counted for the gate): steps 1-8 OK, draft id 458697005 created with --target and a throw-away tag name, 7 assets, sizes equal, 177462-char notes shortened to 107866, draft deleted; afterwards gh release list unchanged, 0 drafts, 0 dryrun tags on origin or local. The real notes file exposed a bug the fake data hid: several 'What changed since' sections and the heading is 'SAT is opt-in'; fixed (cut at the first What-changed section, match '## SAT is '), test added (Body: real note layout), 35 tests.

2026-11-26 otgw-webui dry run #1 on R1 (beta.438, release commit 14539282e): PASS exit 0, steps 1-8 ok, nothing published; finding: the script refused the build dir because two flash zips were in it (+1453928 and the stale +bfae2b8), so I ran on a copy with only the +1453928 files; after the run gh release list showed 0 drafts, no dryrun tags locally or on origin, no v2.0.0-beta.438 tag. R2 preparation (coordinator ask, beta.451 = build 628de05ea, release commit a docs-only commit on top, assets keep githash 628de05): scripts/beta_publish.py gets --build-commit X (the zip githash must be X; the release commit must be X or a descendant with an empty diff under src/, libraries/, scripts/, build.py, platformio.ini; without the flag the normal check is unchanged) and --require-text TEXT (repeatable: the release body must contain it, so the SAT-on check numbers cannot be cut by the shortener) plus a guard that the shortened body keeps the notes' disclaimer block. tests/test_beta_publish.py: +12 tests (BuildCommit 7, RequireText 5), full file 47 OK before the last mutation round and 19 (BuildCommit, RequireText, DryRun) OK after; 4 mutants killed (paths narrowed, githash check dropped, require-text ignored, disclaimer check dropped). Commit not pushed yet (push slot to ask).

2026-11-26 AC3 evidence: dry run #1 on R1 (v2.0.0-beta.438, release commit 14539282e): exit 0, steps 1-8 ok. Dry run #2 on R2 (v2.0.0-beta.451, release commit 2bf4b3ab8, --build-commit 628de05ea, build dir wt-d3-t2/build with only the +628de05 set, ino.bin sha256 fce825dea37d3bd6060fa5d0197f4459b2edd45460bfbebc4894dab503ff4822, notes RELEASE_NOTES_2.0.0-beta.451.md, 5 --require-text anchors incl. 'Not verified: SAT behaviour with a boiler or a thermostat attached'): exit 0, step 3 'build commit 628de05ea is an ancestor of the release commit, nothing changed under src/, libraries/, scripts/, build.py, platformio.ini since', step 4 shortened 188855 -> 109775 characters with every anchor present, step 7 draft id 459383561 prerelease not latest with 7 assets of equal size, step 8 draft deleted. The soak verdict used for #2 was a labelled SCRATCH file (T2 soak verdict due about 14:07), so the real run needs the real verdict. After each run: gh release list 0 drafts, 0 dryrun tags locally and on origin, no v2.0.0-beta.438 / .451 tag on origin. Ledger (OTGW-capture/beta-publish-dryruns.tsv): 2 betas clean, so the two-dry-run gate is met. Not verified: a real publish (not run here, by design).

2026-11-26 13:3x (date) --append-body FILE added at the coordinator's request: the file text goes into the body AFTER the shortening, in front of the footer ('## How to report', else at the end), is itself a required text (cannot be dropped), em dash and empty or missing file are refused. tests/test_beta_publish.py +5 (AppendBody): old script (HEAD) RED 4 errors, fix GREEN 5/5, 3 mutants killed (flag ignored, paragraph after the footer, shortened path forgets the paragraph), whole file 52 OK. Local only on lane ba-rel, not pushed (push after the publish).

2026-11-26 14:1x (date) real runs and dry runs. Dry runs (ledger OTGW-capture/beta-publish-dryruns.tsv, 2 betas clean): #1 v2.0.0-beta.438 on a copy of the build dir (stale +bfae2b8 set made the script refuse, as designed); #2 v2.0.0-beta.451 with a labelled scratch verdict; #3 v2.0.0-beta.451 with the real inputs (preflight, exit 0). Real publishes: v2.0.0-beta.438 was published manually by the coordinator (before the tool had two dry runs); v2.0.0-beta.451 was published BY THE TOOL at 14:17 (date) under the coordinator's invocation of Robert's standing beta permission: tag v2.0.0-beta.451 -> 2bf4b3ab8 (git ls-remote ^{}), release https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.451, 7 assets (flash.zip 6511339, merged-full 4128768, elf 26246140, ino.bin 2162640, littlefs 786432, safeboot 931648, SHA256SUMS 733), step 9 re-download: every sha256 equal to the local file, flash zip sha256 b7a0d6da44f899ebc3ca9fd23df9d8fef3106850becc54d3cc58de33cf3c5520, Latest still v1.7.10, 0 drafts, no dryrun tags. The coordinator re-verified tag, assets, latest flag and the soak paragraph independently. Not covered here: Discord posts and the published-tags.txt ledger (BlueFlame / coordinator).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: refusals and the shortened body are implemented and tested (tests/test_beta_publish.py, red on mutants). AC2: 52 tests with a fake gh, every step and refusal; mutants killed for each added flag (--build-commit 4, --require-text/disclaimer, --append-body 3). AC3: two clean dry runs on real betas (beta.438, beta.451) and a real publish of beta.451 through the tool with the evidence in the notes above (tag commit 2bf4b3ab8, 7 assets re-downloaded and compared by sha256, latest v1.7.10). Added beyond the plan, on the coordinator's request: --build-commit (docs-only release commit on a fix build), --require-text, --append-body. Not verified: a publish with a stale file in the build dir (the tool refuses; no real case exercised), and Discord posting (out of scope).
<!-- SECTION:FINAL_SUMMARY:END -->
