---
id: TASK-1809
title: >-
  beta-release-2x skill: post the #alpha-testing zip BEFORE publishing the
  GitHub release (post script refuses an already-published prerelease)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T07:07:00Z'
updated_date: '2026-11-14T15:26:00Z'
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
- [x] #1 SKILL.md order changed (or script rule changed) with the reason
- [x] #2 A dry run shows the post step passes before publish
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 (95): cause is not the phase order. scripts/discord-post-build.sh:72-74 refuses a key that is in the machine-wide local ledger published-tags.txt; the script makes no GitHub call at all. In the beta.7 model the release commit is docs-only on top of the candidate, so it carries the candidate's beta.N, and the candidate's developer already posted that zip on the alpha track. So the post refuses before or after the publish alike; moving it to Phase 4 (the fix proposed in the description) would not help. Ledger history agrees: beta.174 'zip post refused after publish', beta.224 and beta.237 'zip already posted as alpha beta.N', a note was posted instead.

Change (SKILL.md Phase 6.1 + Known trap 6): look the key up in published-tags.txt / published-posts.tsv first. Found (normal case): no zip post; the coordinator posts an #alpha-testing note with the release link and the earlier zip message id. Not found (release commit with its own bump, e.g. first beta after alphas): post the zip as before. Script unchanged: its one-post-per-tag rule is a maintainer rule (2026-10-31) and stays.

Dry run (scratchpad t1809/dryrun.txt), HEAD f276f9a carries beta.247, dummy zip named for HEAD, temp ledgers via OTGW_PUBLISHED_TAGS: A key in ledger -> 'REFUSED: prerelease 2.0.0-beta.247 was already published', exit 2; B key not in ledger -> 'DRY RUN OK: would post ...beta.247+f276f9a-flash.zip', exit 0; grep for gh/api.github/releases in the script: none. The skill's lookup commands on the real ledger for beta.237 return the key and zip message 1571041876306432021, which the ledger lists as the beta.237 zip post.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: SKILL.md Phase 6.1 now checks the local post ledger first and posts a note instead of the zip when beta.N was already posted (the normal docs-only release commit); Known trap 6 names the refusal text and why moving the post earlier does not help. The order was not changed because the order is not the cause: discord-post-build.sh:72-74 refuses on the local ledger, with no GitHub call. AC2: dry run with temp ledgers: key present -> REFUSED exit 2 (before any publish), key absent -> DRY RUN OK exit 0; lookup commands checked on the real ledger (beta.237 -> zip message 1571041876306432021). Not verified: a real beta flow run with the new text (next public beta).
<!-- SECTION:FINAL_SUMMARY:END -->
