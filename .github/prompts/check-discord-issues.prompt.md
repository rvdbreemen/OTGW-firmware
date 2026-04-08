---
name: check-discord-issues
description: Scan Discord for new OTGW-firmware issues, triage them, and prepare an approved fix workflow.
agent: agent
tools:
   - discord/*
argument-hint: Optional context, for example "focus on beta channels" or "only analyze, do not implement".
---

# Check Discord Issues

Scan the OTGW-firmware Discord server for new user-reported issues since the last check, analyze them, propose a fix, and only implement after explicit developer approval.

## Important limitation

Only execute the Discord-reading steps if Discord tooling is actually available in this Copilot session.

If Discord tools are not available:

1. Stop before any Discord API action.
2. State clearly that the prompt is ready, but Discord access still needs to be configured in Copilot.
3. Tell the developer which tool capability is missing.
4. Offer a fallback where the developer pastes exported Discord messages for analysis.

## Workflow

Follow these phases strictly and in order.

### Phase 1: Login and read Discord messages

1. Login to Discord using the available Discord tool integration.
2. Read the last-checked timestamp from `.claude/discord_last_checked.txt`. If the file does not exist, default to messages from the last 7 days.
3. Read messages from these channels:
   - `#beta-testing` — channel ID `914498730001072149` — limit 50
   - `#devs-esp-firmware` — channel ID `924989767966425158` — limit 50
   - `#english-support` — channel ID `931267109726593116` — limit 50
   - `#nederlandse-ondersteuning` — channel ID `815561033036333076` — limit 50
4. Filter messages to only those posted after the last-checked timestamp.
5. Exclude messages from the maintainer with user ID `384411356616720384`, username `number3nl`, and exclude bot accounts.
6. Save the current timestamp to `.claude/discord_last_checked.txt` for the next run.

### Phase 2: Identify and triage issues

1. Classify each message as one of: bug report, feature request, question, general discussion, or not relevant.
2. Focus only on bug reports and actionable issues.
3. If no new issues are found, report `No new issues since last check` and stop.
4. Present a numbered list of identified issues with:
   - channel name
   - reporter username, with trailing 4-digit suffix stripped for display when applicable
   - short issue summary
   - relevant excerpts or reply context

Checkpoint: ask the developer which issue or issues to work on. Do not proceed until they select one.

### Phase 3: Analyze the selected issue

1. Read the related conversation or thread for full context.
2. Search the codebase for relevant code paths.
3. Draft a suggested solution with:
   - likely root cause
   - proposed fix
   - affected files
   - possible risks or side effects
   - testing approach
4. Present the plan to the developer.

Checkpoint: wait for developer approval before editing any files.

### Phase 4: Branch and version preparation

1. Check whether the working tree is clean.
2. If there are uncommitted changes, warn the developer and stop.
3. Derive a short kebab-case description from the issue, maximum 5 words.
4. Create a branch from `dev` named `fix-issue-<short-description>`.
5. Bump the patch version in `src/OTGW-firmware/version.h` and refresh derived version strings consistently.
6. Run `python scripts/autoinc-semver.py src/OTGW-firmware --filename version.h --update-all --increment-build 0`.
7. Commit the version bump separately.

### Phase 5: Implement the fix

1. Follow the approved plan.
2. Respect OTGW-firmware project rules from workspace instructions, especially:
   - use PROGMEM helpers for string literals
   - avoid `String` in hot paths
   - never write to `Serial`
   - use the OTGW command queue where applicable
   - validate buffer sizes
   - feed the watchdog in long-running loops
3. Build with `python build.py --firmware`.
4. Run `python evaluate.py --quick`.
5. Fix relevant build or evaluation problems before proceeding.

### Phase 6: Report results

1. Commit the fix with a descriptive message.
2. Present a summary including:
   - the Discord issue being addressed
   - what changed
   - build status
   - evaluation status
   - branch name
   - follow-up risks or open items
3. Do not push to remote unless the developer explicitly asks.

## Rules

1. Never skip checkpoints.
2. Never push without explicit permission.
3. Never use destructive git commands.
4. Keep release-related text in English.
5. If Discord access is unavailable, stop early and explain the missing capability instead of pretending the scan succeeded.