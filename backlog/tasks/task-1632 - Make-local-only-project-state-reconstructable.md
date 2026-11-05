---
id: TASK-1632
title: Make local-only project state reconstructable
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T22:43:00Z'
updated_date: '2026-11-05T04:38:00Z'
labels:
  - process
  - agentic
dependencies: []
priority: medium
ordinal: 759000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Finding F3 (bus factor). Critical state lives only on Robert's Windows machine and is gitignored: .claude/alpha-build-ledger.local.md (which alpha tags were posted), .claude/discord-people.local.md (people and their rules), the Discord webhook URL in %LOCALAPPDATA%, the discord-mcp launcher and token. Without that machine the publication and Discord paths stop and the ledger cannot be rebuilt.
Do not put secrets or per-person rules in the public repo.
Context: docs/process/agentic-setup.md (finding numbers F1-F7) and docs/process/agentic-improvements-handoff-2026-11-04.md. Parent: TASK-1624.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A script rebuilds the alpha ledger from git log -G_VERSION_PRERELEASE on origin/dev plus the zip posts (Discord read or a local post log); its output matches the current ledger for the last 10 tags
- [x] #2 An anonymised template of discord-people.local.md is versioned (roles and fields, no names or IDs)
- [x] #3 A runbook in docs/guides/ describes setting up discord-mcp, the webhook and the bench config on a second machine
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05. scripts/alpha_ledger.py rebuilds the ledger from git (bump commits on origin/dev: tag, commit, date, TASK ids) plus published-tags.txt and the new published-posts.tsv. discord-post-build.sh now appends a TSV post record after every successful post (UTC time, tag, version+hash, message id, -m text with tabs/newlines removed; tested in isolation: 5 columns; bash -n OK). Not-posted tags are shown as covered either by a post whose -m names them (ranges like beta.57-60 understood) or, per the CLAUDE.md protocol, by the next posted zip. Comparison with the hand ledger (.claude/alpha-build-ledger.local.md, last edited 2026-11-03 20:36, so it stops at beta.60) for its last 10 tags beta.51-60: posted state and covered-by groups match (50->51, 53->54, 55->56, 57-59->60); every task of a bump commit is in the ledger's task list; the commit column matches for beta.52, 54 and 60 and differs for the groups where the hand ledger records the later build HEAD instead of the bump commit (26be8ce, 5cdf2ff/125ed81), which is expected. Finding: the hand ledger has not been updated for beta.61-79. .claude/discord-people.example.md (anonymised template, same sections and columns, no names or IDs). docs/guides/second-machine-setup.md lists every local-only file, where it lives and how to recreate it.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Local-only state is reconstructable. AC1: scripts/alpha_ledger.py reproduces the hand ledger's last 10 tags (beta.51-60) in posted state, covered-by groups and tasks; the commit column differs only where the hand ledger records the later build HEAD. discord-post-build.sh now writes published-posts.tsv so message ids are kept from now on. AC2: .claude/discord-people.example.md, anonymised. AC3: docs/guides/second-machine-setup.md. Side finding: the hand ledger stops at beta.60; the script fills that gap.
<!-- SECTION:FINAL_SUMMARY:END -->
