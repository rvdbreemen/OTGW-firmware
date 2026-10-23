---
id: TASK-1313
title: 'Post the combo flash zip to #alpha-testing via webhook after each build'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-23T08:54:00Z'
updated_date: '2026-10-23T11:38:00Z'
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
- [x] #1 scripts/discord-post-build.sh posts build/OTGW-firmware-esp32-combo-<ver>+<hash>-flash.zip with a one-line message (version, githash, task id) and exits non-zero on any non-2xx
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

AC1 evidence: maintainer ran scratchpad pipeline-test.sh (real script, clean throwaway repo, 2 KB dummy zip): 'POSTED: OTGW-firmware-esp32-combo-0.0.0-pipelinetest.1+b510706-flash.zip (2048 bytes), http 200, message 1563153944753217537', exit 0. Test message deleted via webhook: http 204. Allow rule 'Bash(bash scripts/discord-post-build.sh:*)' added to .claude/settings.local.json on the maintainer's request. Note: settings.local.json already held 'Bash(*)' when the classifier denied the earlier run, so whether the narrow rule lets an auto-mode cycle post is unverified until the first real step 9. AC5 still open.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Posts the esp32-combo flash zip to #alpha-testing through a Discord webhook after each task build.
- scripts/discord-post-build.sh: refuses (exit 2) on zip githash != HEAD, dirty src/, zip tag != HEAD _VERSION_PRERELEASE, zip > 10 MB; URL read from the out-of-repo file and never printed; payload via --form-string; prints the message id.
- implement-next-task step 9 calls it; release and beta-prerelease skills untouched (1.x / #beta-testing, no 2.0.0 zip).
Evidence: AC1 maintainer-run real post via the script (http 200, msg 1563153944753217537, deleted 204); AC2/3 tests/test_discord_post_build.py 10/10 OK, mutants red (3 and 1 FAIL), live dry-run on the dirty tree refused exit 2; AC4 SKILL.md diff in 711feb333/8c30e6fa5; evaluate.py --quick exit 0.
Not verified: AC5, a real post of a clean build. Blocked: another session holds uncommitted TASK-1314 work under src/ and emptied build/. It completes at the first step 9 after that; whether the allow rule lets an auto-mode run post is also first proven there.
<!-- SECTION:FINAL_SUMMARY:END -->
