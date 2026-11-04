---
id: TASK-1632
title: Make local-only project state reconstructable
status: To Do
assignee: []
created_date: '2026-11-04T22:43:00Z'
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
- [ ] #1 A script rebuilds the alpha ledger from git log -G_VERSION_PRERELEASE on origin/dev plus the zip posts (Discord read or a local post log); its output matches the current ledger for the last 10 tags
- [ ] #2 An anonymised template of discord-people.local.md is versioned (roles and fields, no names or IDs)
- [ ] #3 A runbook in docs/guides/ describes setting up discord-mcp, the webhook and the bench config on a second machine
<!-- AC:END -->
