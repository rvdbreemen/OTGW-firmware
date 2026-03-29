# /check_discord_issues — Monitor Discord for user-reported issues

Scan the OTGW-firmware Discord server for new issues reported by users since the last check, analyze them, propose a fix, and implement it on a dedicated branch after developer approval.

## Workflow

Follow these phases strictly and in order.

### Phase 1: Login and read Discord messages

1. **Login to Discord** using `mcp__discord__discord_login`.
2. **Read the last-checked timestamp** from `.claude/discord_last_checked.txt`. If the file does not exist, default to messages from the last 7 days.
3. **Read messages** from the following channels (use `mcp__discord__discord_read_messages`):
   - `#beta-testing` — channel ID `914498730001072149` (limit 50)
   - `#devs-esp-firmware` — channel ID `924989767966425158` (limit 50)
   - `#english-support` — channel ID `931267109726593116` (limit 50)
   - `#nederlandse-ondersteuning` — channel ID `815561033036333076` (limit 50)
4. **Filter messages** to only those posted after the last-checked timestamp.
5. **Exclude** messages from the maintainer (user ID `384411356616720384`, username `number3nl`) and bot accounts.
6. **Save the current timestamp** to `.claude/discord_last_checked.txt` for next run.

### Phase 2: Identify and triage issues

1. **Classify each message** as one of: bug report, feature request, question, general discussion, or not relevant.
2. **Focus only on bug reports and actionable issues.** Skip feature requests, questions, and general chat.
3. If **no new issues** are found, report "No new issues since last check" and stop.
4. **Present a numbered list** of identified issues to the developer with:
   - Channel where it was reported
   - Reporter username (strip trailing 4-digit suffixes for display, e.g. `fuzzyduck3793` -> `fuzzyduck`)
   - Short summary of the issue
   - Relevant message excerpts or context from replies

**CHECKPOINT: Ask the developer which issue(s) to work on. Do not proceed until they select one.**

### Phase 3: Analyze the selected issue

1. **Read all related messages** in the thread/conversation for full context.
2. **Search the codebase** for relevant code paths related to the reported issue.
3. **Draft a suggested solution** with:
   - Root cause analysis (what's likely going wrong)
   - Proposed fix (which files to change, what to change)
   - Potential risks or side effects
   - Testing approach
4. **Present the plan** to the developer.

**CHECKPOINT: Wait for developer approval of the plan. Do not write any code until approved. Adjust the plan if the developer requests changes.**

### Phase 4: Create branch and bump version

1. **Ensure working tree is clean** — check `git status`. If there are uncommitted changes, warn the developer and stop.
2. **Derive a short kebab-case description** from the issue (max 5 words, e.g., `mqtt-reconnect-crash`, `ot-log-missing-data`).
3. **Create and checkout a new branch** from `dev`:
   ```
   git checkout dev
   git pull
   git checkout -b fix-issue-<short-description>
   ```
4. **Bump the patch version** in `src/OTGW-firmware/version.h`:
   - Increment `_VERSION_PATCH` by 1 (e.g., 2 -> 3)
   - Update ALL related defines consistently:
     - `_SEMVER_CORE` — `"X.Y.Z"`
     - `_SEMVER_BUILD` — `"X.Y.Z+BUILD"`
     - `_SEMVER_GITHASH` — `"X.Y.Z+HASH"`
     - `_SEMVER_FULL` — `"X.Y.Z-beta+HASH"`
     - `_SEMVER_NOBUILD` — `"X.Y.Z-beta (DATE)"`
     - `_VERSION` — `"X.Y.Z-beta+HASH (DATE)"`
   - Keep `_VERSION_BUILD`, `_VERSION_GITHASH`, `_VERSION_DATE`, `_VERSION_TIME` unchanged (the build system updates these)
5. **Run `python scripts/autoinc-semver.py`** from the `src/OTGW-firmware/` directory to update build number, githash, and timestamps:
   ```
   python scripts/autoinc-semver.py src/OTGW-firmware --filename version.h --update-all --increment-build 0
   ```
   Using `--increment-build 0` ensures only the derived strings and hash are refreshed without bumping the build number again.
6. **Commit the version bump**:
   ```
   git add src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
   git commit -m "chore: bump version to vX.Y.Z-beta for fix-issue-<description>"
   ```

### Phase 5: Implement the fix

1. **Follow the approved plan** from Phase 3.
2. **Respect all project coding rules** from CLAUDE.md:
   - Use PROGMEM (`F()`, `PSTR()`, `snprintf_P`) for all string literals
   - No `String` class in hot paths
   - Use `char[]` buffers with `strlcpy`, `strncat`, `snprintf_P`
   - Never write to Serial (use `DebugTln()`, `DebugTf()`)
   - Use `addOTWGcmdtoqueue()` for OTGW commands
   - Feed watchdog in long loops: `feedWatchDog()`
   - Validate buffer sizes before string operations
3. **Run the build** to verify compilation:
   ```
   python build.py --firmware
   ```
4. **Run the evaluator** to check code quality:
   ```
   python evaluate.py --quick
   ```
5. **Fix any build or evaluation errors** before proceeding.

### Phase 6: Report results

1. **Commit all changes** with a descriptive message referencing the Discord issue.
2. **Present a summary** to the developer:
   - What was the issue (from Discord)
   - What was changed (files modified, approach taken)
   - Build status (pass/fail)
   - Evaluation status (pass/fail)
   - Branch name for review
   - Any follow-up items or risks
3. **Do NOT push** the branch — let the developer review first.

## Important rules

- **Never skip a checkpoint** — always wait for developer approval
- **Never push to remote** without explicit developer permission
- **Never force-push** or use destructive git commands
- **All code changes must follow OTGW-firmware coding rules** (PROGMEM, no String, etc.)
- **All release-related text must be in English** (international audience)
- **Strip Discord username suffixes** for display (4-digit trailing numbers)
