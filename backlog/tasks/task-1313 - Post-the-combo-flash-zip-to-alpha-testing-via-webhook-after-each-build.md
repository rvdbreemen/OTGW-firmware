---
id: TASK-1313
title: 'Post the combo flash zip to #alpha-testing via webhook after each build'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T08:54:00Z'
updated_date: '2026-10-23T12:17:00Z'
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
- [x] #5 Evidence: one real post of a clean build in #alpha-testing, plus the refusal shown on a dirty tree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Experiments 2026-10-23: webhook GET -> BlueFlame Build Bot, channel 1514720723980259460. Text upload http 200, readback, DELETE 204, GET after 404. 6 MB dummy zip http 200 (6000406 B in 2.4 s), deleted 204. Bot REST route: bot has ATTACH_FILES but the auto-mode classifier blocks a bot-token POST (Credential Exploration). Chrome route: extension not connected.
Script scripts/discord-post-build.sh + tests/test_discord_post_build.py: 8/8 OK on the real script; mutant without the dirty and hash refusals: 3 FAIL (dirty, untracked, hash-not-HEAD). Live dry-run on the real tree (another session's uncommitted TASK-1314 work under src/): REFUSED exit 2, listing the 5 files.
SKILL.md step 9 now calls the script.
OPEN: the script's real post path was denied by the auto-mode classifier ('judged dangerous'). The automated cycle needs a maintainer allow rule for 'bash scripts/discord-post-build.sh*' before AC1/AC5 can be evidenced. Not added by Claude: permission config is the maintainer's call.

Advisor round: payload now via --form-string (a ';' or '@' in -m no longer reaches curl's form parser), control characters stripped, dry-run prints the payload, POSTED line prints the message id, zip prerelease must equal HEAD's _VERSION_PRERELEASE (version.h is outside the dirty check). Tests 10/10 OK; mutant without the bump check: 1 FAIL (test_uncommitted_bump_refused). evaluate.py --quick exit 0, 0 FAIL. Scope: release and beta-prerelease skills untouched on purpose: 1.x public channels and #beta-testing (assets via CI/GitHub release), this webhook reaches #alpha-testing only.

AC1 evidence: maintainer ran scratchpad pipeline-test.sh (real script, clean throwaway repo, 2 KB dummy zip): 'POSTED: OTGW-firmware-esp32-combo-0.0.0-pipelinetest.1+b510706-flash.zip (2048 bytes), http 200, message 1563153944753217537', exit 0. Test message deleted via webhook: http 204. Allow rule 'Bash(bash scripts/discord-post-build.sh:*)' added to .claude/settings.local.json on the maintainer's request. Note: settings.local.json already held 'Bash(*)' when the classifier denied the earlier run, so whether the narrow rule lets an auto-mode cycle post is unverified until the first real step 9. AC5 still open.

2026-10-23 14:20 field failure (session 2cc0fd9b, Git Bash via the Bash tool, alpha.465+7f4e9a4 zip): 'curl: (26) Failed to open/read local data from file/application' -> 'FAILED: curl error'. Cause: curl here is the native mingw build (/ucrt64/bin/curl 8.22.0); Git Bash does not convert the MSYS path inside the -F argument 'files[0]=@/d/...', and mingw curl cannot open '/d/...'. Verified: the same -F with $(cygpath -w "$zip") opens the file (curl then fails only on the test host). Fix: on MSYS/Git Bash pass the zip as $(cygpath -w "$zip") (guard with 'command -v cygpath'). Not worked around (no manual/MCP upload); the alpha.465 zip was not posted.

2026-10-23 14:30 (session 2cc0fd9b): fix 5cf2758c4 passes the zip to curl as $(cygpath -w) when cygpath exists. Evidence: real post of the clean HEAD build OTGW-firmware-esp32-combo-2.0.0-alpha.465+5cf2758-flash.zip -> 'POSTED ... http 200, message 1563164311466283070', exit 0 (before the fix: curl error 26 on the 7f4e9a4 zip). Refusals: zip githash 7f4e9a4 vs HEAD a3ba63257 -> 'REFUSED: zip githash ... is not HEAD' ; uncommitted edit in src/OTGW-firmware/PICreturnSensor.h -> 'REFUSED: uncommitted changes under src/', exit 2 (an edit to version.h alone is excluded by design: build stamps).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/discord-post-build.sh posts the combo flash zip to #alpha-testing through the out-of-repo webhook, with the version/githash/task line; it refuses a zip whose githash is not HEAD, uncommitted src/ changes (build stamps in version.h/version.hash excepted) and zips above 10 MB, and never prints the URL. Called from step 9 of implement-next-task. Evidence per AC: AC1/AC5 real post http 200 (message 1563164311466283070) of alpha.465+5cf2758; AC2 both refusals shown (githash mismatch, dirty src file, exit 2); AC3/AC4 earlier session notes (size gate, URL file, SKILL.md). Fix in this session: Git Bash's native curl could not read the MSYS zip path (curl error 26), now handed a Windows path via cygpath -w.
<!-- SECTION:FINAL_SUMMARY:END -->
