---
id: TASK-1313
title: 'Post the combo flash zip to #alpha-testing via webhook after each build'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T08:54:00Z'
updated_date: '2026-10-23T11:32:00Z'
labels: []
dependencies: []
ordinal: 436000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Automate the one manual step left in the build cycle: uploading the esp32-combo flash zip to #alpha-testing. Experiment 2026-10-23 proved the Discord webhook route (BlueFlame Build Bot, channel 1514720723980259460): text upload http 200 + readback + delete 204/404; 6 MB dummy zip http 200 in 2.4 s (limit 10 MB, guild premium tier 0). Webhook URL lives outside the repo in %LOCALAPPDATA%\OTGW-capture\discord-alpha-webhook.url. Rejected routes: Chrome self-bot (extension not connected, Discord ToS), MCP send (no file parameter); bot REST also has ATTACH_FILES but needs the bot token on the command line.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/discord-post-build.sh posts build/OTGW-firmware-esp32-combo-<ver>+<hash>-flash.zip with a one-line message (version, githash, task id) and exits non-zero on any non-2xx
- [x] #2 Script refuses when the working tree has uncommitted changes under src/ or when the zip githash differs from HEAD (no build whose hash lies)
- [x] #3 Script refuses a zip above 10 MB and reads the webhook URL only from the out-of-repo file; the URL never appears in output or logs
- [x] #4 implement-next-task SKILL.md calls the script at the 'leave the combo zip ready' step
- [ ] #5 Evidence: one real post of a clean build in #alpha-testing, plus the refusal shown on a dirty tree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Experiments 2026-10-23: webhook GET -> BlueFlame Build Bot, channel 1514720723980259460. Text upload http 200, readback, DELETE 204, GET after 404. 6 MB dummy zip http 200 (6000406 B in 2.4 s), deleted 204. Bot REST route: bot has ATTACH_FILES but the auto-mode classifier blocks a bot-token POST (Credential Exploration). Chrome route: extension not connected.
Script scripts/discord-post-build.sh + tests/test_discord_post_build.py: 8/8 OK on the real script; mutant without the dirty and hash refusals: 3 FAIL (dirty, untracked, hash-not-HEAD). Live dry-run on the real tree (another session's uncommitted TASK-1314 work under src/): REFUSED exit 2, listing the 5 files.
SKILL.md step 9 now calls the script.
OPEN: the script's real post path was denied by the auto-mode classifier ('judged dangerous'). The automated cycle needs a maintainer allow rule for 'bash scripts/discord-post-build.sh*' before AC1/AC5 can be evidenced. Not added by Claude: permission config is the maintainer's call.

Advisor round: payload now via --form-string (a ';' or '@' in -m no longer reaches curl's form parser), control characters stripped, dry-run prints the payload, POSTED line prints the message id, zip prerelease must equal HEAD's _VERSION_PRERELEASE (version.h is outside the dirty check). Tests 10/10 OK; mutant without the bump check: 1 FAIL (test_uncommitted_bump_refused). evaluate.py --quick exit 0, 0 FAIL. Scope: release and beta-prerelease skills untouched on purpose: 1.x public channels and #beta-testing (assets via CI/GitHub release), this webhook reaches #alpha-testing only.
<!-- SECTION:NOTES:END -->
