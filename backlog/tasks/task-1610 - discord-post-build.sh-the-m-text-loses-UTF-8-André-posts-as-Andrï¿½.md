---
id: TASK-1610
title: 'discord-post-build.sh: the -m text loses UTF-8 (André posts as Andrï¿½)'
status: To Do
assignee: []
created_date: '2026-11-04T19:47:00Z'
labels:
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 750000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
beta.79 #alpha-testing post msg 1567619129895034960 shows 'Andr� Brait' (seen through discord-mcp read; check in the Discord client). Likely the -m argument is passed through Git Bash/curl without UTF-8. Workaround: ASCII in -m.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A post with a non-ASCII -m text (e.g. André) shows correctly in Discord (read back)
- [ ] #2 Host test of the JSON payload encoding in scripts/discord-post-build.sh
<!-- AC:END -->
