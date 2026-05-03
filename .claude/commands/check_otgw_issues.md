# /check_otgw_issues — Monitor Discord, GitHub and Tweakers for user-reported issues

Scan the OTGW-firmware Discord server **and** GitHub issue tracker for new issues reported by users since the last check, analyze them, propose a fix, and implement it on a dedicated branch after developer approval.

## Workflow

Follow these phases strictly and in order.

### Phase 1: Read Discord messages

The Discord MCP server runs as the `discord-mcp` Docker container on `http://localhost:8085/mcp` with `DISCORD_TOKEN` preloaded from the host environment — there is **no separate login step**. The first `read_messages` call doubles as the connection check. **Tool namespace is `mcp__discord-mcp__*`.** Always use these MCP tools, never curl or direct Discord API calls (curl is fine for the CDN attachment downloads in Phase 1d — see below).

1. **Read the last-checked timestamp** from `.claude/discord_last_checked.txt`. If the file does not exist, default to messages from the last 7 days.
2. **Read messages** from the following channels using `mcp__discord-mcp__read_messages` with `count="50"`:
   - `#beta-testing` — `channelId="914498730001072149"`
   - `#devs-esp-firmware` — `channelId="924989767966425158"`
   - `#english-support` — `channelId="931267109726593116"`
   - `#nederlandse-ondersteuning` — `channelId="815561033036333076"`

   The response payload includes per-message **attachment metadata** (attachment ID, filename, MIME type, size, signed CDN URL). The separate `mcp__discord-mcp__get_attachment` tool returns the same info and is redundant.
3. **Filter messages** to only those posted after the last-checked timestamp.
4. **Exclude** messages from the maintainer (user ID `384411356616720384`, username `number3nl`) and bot accounts.
5. **Save the current timestamp** to `.claude/discord_last_checked.txt` for next run.

### Phase 1b: Fetch GitHub issues

1. **Read the last-checked GitHub timestamp** from `.claude/github_last_checked.txt`. If the file does not exist, default to the last 7 days.
2. **List new open GitHub issues** created or updated since the last check:

   ```bash
   gh issue list --repo rvdbreemen/OTGW-firmware --state open --limit 50 --json number,title,body,createdAt,updatedAt,labels,author,url
   ```

3. **Filter** to issues created or updated after the last-checked GitHub timestamp.
4. **Exclude** issues authored by the maintainer (`rvdbreemen`) or bot accounts (login contains `[bot]`).
5. **For each new issue**, fetch its comments for full context:

   ```bash
   gh issue view <number> --repo rvdbreemen/OTGW-firmware --json number,title,body,comments,labels,author,url
   ```
6. **Save the current timestamp** to `.claude/github_last_checked.txt` for next run.

### Phase 1c: Fetch Tweakers forum posts

1. **Read the last-checked Tweakers timestamp** from `.claude/tweakers_last_checked.txt`. If the file does not exist, default to posts from the last 7 days.
2. **Fetch the Tweakers RSS feed** using curl and pipe through Python to strip surrogate characters before parsing (WebFetch is blocked by Tweakers; direct XML parse of the raw bytes fails on Windows with Python 3.14+ due to surrogate chars `\udc8d` in the feed):

   ```bash
   curl -s --max-time 10 -A "Mozilla/5.0" "https://gathering.tweakers.net/rss/list_messages/1653967" > .tmp/tweakers_rss.bin
   python3 -c "
   data = open('.tmp/tweakers_rss.bin', 'rb').read().decode('utf-8', errors='replace')
   open('.tmp/tweakers_rss.xml', 'w', encoding='utf-8').write(data)
   "
   ```

   Then parse `.tmp/tweakers_rss.xml` as UTF-8 text. The `errors='replace'` step ensures surrogate bytes become `\ufffd` (replacement character) instead of raising `UnicodeEncodeError`.

3. **Parse the RSS XML** to extract individual `<item>` elements with:
   - `<dc:creator>` — post author (username)
   - `<pubDate>` — post timestamp (RFC 822 format)
   - `<description>` or `<content:encoded>` — post content
   - `<link>` — direct permalink to the post
4. **Filter** to only posts with `<pubDate>` newer than the last-checked Tweakers timestamp.
5. **Exclude** posts by the maintainer (Tweakers username `number3` or `rvdbreemen`) or purely social/off-topic messages.
6. **Save the current timestamp** to `.claude/tweakers_last_checked.txt` for next run.
7. **Note**: Tweakers is a Dutch forum — posts will be in Dutch. Summarize them in English for the triage list.

### Phase 1d: Fetch attachment contents (logs, screenshots)

Discord support channels (especially `#beta-testing`) carry the bulk of the **diagnostic evidence** as attached files: telnet logs, MQTT captures, screenshots of failing UIs. **Triage without reading the attachment is triage on partial information.** Earlier replies that said "I cannot read attachments through the bot, only message text" are obsolete; the bot can now both fetch and analyse them.

For every message that survives Phase 1 filtering and carries one or more attachments, fetch the contents before drafting the triage entry. Same applies to GitHub issue bodies/comments that link CDN-hosted screenshots or paste log gists, and to Tweakers posts with image links.

**Procedure depends on attachment type and analysis goal:**

| Type | Goal | How |
|---|---|---|
| Text (`.txt`, `.log`, `.json`, `.md`) | AI-summarised quick read | `WebFetch(url, prompt="…")` — small model returns processed/summarised answers, not always verbatim |
| Text (`.txt`, `.log`, `.json`, `.md`) | Verbatim, line-precise diagnosis or `Grep` over content | PowerShell download → `Read` / `Grep` on the local file |
| Image (`.png`, `.jpg`, `.webp`) | See the screenshot, extract on-screen text or layout | PowerShell download → `Read` on the **Windows path** (Read can open images in Claude Code) |

**Download recipe (Windows-safe — do NOT use Git-Bash `/tmp/`, the Read tool cannot resolve those paths):**

```powershell
$dir = "$env:TEMP\otgw-issues-attach"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Invoke-WebRequest -Uri "<signed CDN URL from message>" -OutFile "$dir\<filename>" -UseBasicParsing
```

Then call `Read` with the full Windows path, e.g. `C:\Users\rvdbr\AppData\Local\Temp\otgw-issues-attach\<filename>`. To grep a long log: `Grep` on that same path.

**Caveats:**

- Discord CDN URLs are signed with `ex=<hex-epoch>` and expire roughly 7 days after the message was posted. If the download returns 403, request a fresh signed URL via `mcp__discord-mcp__read_messages` (Discord rolls a new one each call) or ask the poster to re-share.
- `WebFetch` runs the content through a small AI model — for **forensic / line-precise** log diagnosis prefer the download-and-`Grep` route. Use `WebFetch` only for the "what does this log roughly show" first-pass question.
- Windows `$env:TEMP` rotates on its own; no clean-up needed. Don't commit downloaded attachments anywhere.

**Triage output (Phase 2) MUST include a one-line attachment summary** for each item that carried evidence: e.g. `"Attached telnet log (58 KB, build 168bd9e): five clean SAT cycles, then stale-temp fallback at 20:33:54 driving room=0.0 → CS=62.0"`. This is what makes the triage actionable rather than cosmetic.

### Phase 2: Identify and triage issues

Combine Discord messages, GitHub issues, and Tweakers forum posts into a single triage list.

1. **Classify each item** as one of: bug report, feature request, question, general discussion, or not relevant.
2. **Focus only on bug reports and actionable issues.** Skip feature requests, questions, and general chat.
3. If **no new issues** are found from any source, report "No new issues since last check" and stop.
4. **Present a numbered list** of identified issues to the developer. For each item include:
   - **Source**: Discord (channel name), GitHub (issue number + link, e.g. `GitHub #542`), or Tweakers (post link)
   - **Reporter**: Discord username (strip trailing 4-digit suffixes), GitHub username, or Tweakers username
   - **Summary**: short description of the issue (in English, even if the original post was in Dutch)
   - **Excerpt**: relevant message or issue body snippet, plus any key replies/comments
5. **Cross-reference**: if the same issue appears in multiple sources, merge them into one entry and note all sources.

### Phase 2b: Backlog triage for every identified issue

For **every** bug report or actionable issue found in Phase 2 — regardless of whether the developer selects it for immediate work — do the following:

1. **Check the backlog** for an existing task covering this issue:

   ```bash
   backlog search "<short issue description>" --plain
   ```

2. **If no task exists**, create one immediately:

   ```bash
   backlog task create "Fix: <short description>" \
     -d "<what the issue is, who reported it, where>" \
     --ac "<testable acceptance criterion>" \
     -l "bug,needs-info" --priority medium
   ```

   Add a reference to the source (GitHub issue URL, Discord channel + username):

   ```bash
   backlog task edit <id> --ref "https://github.com/rvdbreemen/OTGW-firmware/issues/NNN"
   backlog task edit <id> --ref "Discord #channel, user <username>, <date>"
   ```

3. **Assess information readiness.** A task is ready to pick up only when ALL of the following are available:
   - A reproducible description of the problem
   - Enough context to identify the likely code area (logs, MQTT traces, or telnet output)
   - At least one reporter who can validate a fix

4. **If information is insufficient**, keep the task in `To Do` with label `needs-info` and add a note:

   ```bash
   backlog task edit <id> --append-notes "Waiting for: <what is missing, e.g. telnet logs from reporter>"
   ```

   **Do NOT move such a task to In Progress.** It stays `To Do / needs-info` until the missing information arrives.

5. **If information is sufficient**, remove the `needs-info` label and present the task to the developer as a candidate for work.

6. **When checking issues in future runs**, always cross-reference new messages against open `needs-info` tasks in the backlog. If new information has arrived (e.g., logs shared on Discord), update the task notes and re-assess readiness:

   ```bash
   backlog task edit <id> --append-notes "<date>: Reporter shared telnet logs. Ready to investigate."
   backlog task edit <id> -l "bug"   # remove needs-info once sufficient
   ```

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

1. **Commit all changes** with a descriptive message referencing the source issue:
   - If the issue originated from GitHub, include `Fixes #<number>` (or `Refs #<number>` if not fully resolved) in the commit message body so GitHub auto-closes or links the issue.
   - If the issue originated from Discord only, describe it in plain text.
   - Example: `fix: resolve MQTT reconnect crash after router reboot\n\nFixes #542`
2. **Present a summary** to the developer:
   - What was the issue (source: Discord channel or GitHub #NNN + link)
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
- **GitHub issue numbers** must be referenced in commit messages when fixing a GitHub-tracked issue (`Fixes #NNN`)
- **Tweakers posts are in Dutch** — always summarize them in English for the triage list and any developer-facing output
- **Tweakers maintainer username** is `rvdbreemen` — exclude posts by this account from new issue detection
