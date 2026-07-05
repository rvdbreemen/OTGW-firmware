---
id: TASK-1026
title: Analysis + bake-in optimum + ADR + bump/commit
status: To Do
assignee: []
created_date: '2026-07-05 22:23'
labels: []
dependencies: []
ordinal: 237000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Select final N (robustness-first, latency tiebreak). Bake into REST_MAX_INFLIGHT + WEB_FILE_MAX_INFLIGHT + client MAX_INFLIGHT. Write ADR (method+data+chosen N, supersedes TASK-1014 single-flight rule). Update CLAUDE.md rule to 'at most N'. Bump prerelease + commit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Final N baked into firmware caps + client
- [ ] #2 ADR written via adr-kit + accepted
- [ ] #3 CLAUDE.md single-flight rule updated to at-most-N; bump+commit
<!-- AC:END -->
