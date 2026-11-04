---
id: TASK-1610
title: 'discord-post-build.sh: the -m text loses UTF-8 (André posts as Andrï¿½)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T19:47:00Z'
updated_date: '2026-11-04T20:06:00Z'
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
- [x] #2 Host test of the JSON payload encoding in scripts/discord-post-build.sh
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04: cause: the payload reached the Windows-native curl.exe as a --form-string argument, and Windows converts argv through the ANSI code page. Fix: scripts/discord-post-build.sh builds the JSON with Python json.dumps(ensure_ascii=True) from stdin bytes (\uXXXX escapes), so the argument is pure ASCII. tests/test_discord_post_payload.py (cuts the real lines, runs them in Git Bash from a file): fix 2 OK; old script: the accent case FAILS with raw bytes Andr\xc3\xa9 in the payload, the plain case passes on both. AC1 waits for the next real post with a non-ASCII -m text (read back).
<!-- SECTION:NOTES:END -->
