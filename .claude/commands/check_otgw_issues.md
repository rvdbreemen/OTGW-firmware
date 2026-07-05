# /check_otgw_issues — Monitor Discord, GitHub and Tweakers for user-reported issues

Scan the OTGW-firmware Discord server, GitHub issue tracker, and Tweakers forum for new issues since the last check. Triage them, create backlog tasks, and implement fixes after developer approval.

## Token-efficiency rules (read these first)

These rules apply throughout all phases and override any "fetch everything" instinct:

- **O7 Early-exit**: Before any API call, check all three timestamp files. If ALL are < 2 hours old, print "Nothing new since last check (all sources checked < 2h ago)" and stop immediately.
- **O2 Discord limit**: Always use `limit=25`, not 50. If the channel has an `after` parameter, pass the last-checked message ID (convert ISO timestamp to snowflake if needed) to filter server-side.
- **O3 GitHub lazy comments**: Classify each issue from its body first (body is included in the list JSON). Only call `gh issue view` for issues already classified as bug reports. Cap at first 5 comments via `--jq '.comments[:5]'`.
- **O4 Tweakers description-only**: Parse only `<description>` for triage. Only read `<content:encoded>` for items that pass the bug-report keyword filter.
- **O1 Attachment lazy-load**: Attachments are fetched in two passes — see Phase 1d. Never `Read` a full log file; always `Grep` with targeted patterns.
- **O5 Backlog bulk query**: Run `backlog task list -l bug --plain` once at the start of Phase 2b. Match issues locally against this list instead of running `backlog search` per issue.
- **O6 Compact triage**: Write Phase 2 triage output to `.tmp/triage_result.json`. Present one-line summaries to the developer; full excerpts are available on request.

---

## Phase 0: Early-exit check (O7)

```python
# Pseudocode — implement as a Python one-liner or bash check
import datetime, os
files = ['.claude/discord_last_checked.txt', '.claude/github_last_checked.txt', '.claude/tweakers_last_checked.txt']
now = datetime.datetime.utcnow().replace(tzinfo=datetime.timezone.utc)
ages = []
for f in files:
    if os.path.exists(f):
        ts = open(f).read().strip()
        ages.append((now - datetime.datetime.fromisoformat(ts.replace('Z','+00:00'))).total_seconds())
    else:
        ages.append(9999999)
if all(a < 7200 for a in ages):
    print("Nothing new since last check (all sources checked < 2h ago)")
    # STOP
```

If all sources are recent: stop here. No API calls.

---

## Phase 1: Read Discord messages (O2)

**Tool namespace**: `mcp__discord-mcp__*`. No login step — `DISCORD_TOKEN` is injected via MCP config.

1. Read `.claude/discord_last_checked.txt`. Default to 7 days ago if absent.
2. Fetch messages from these channels with **`limit=25`**:
   - `#beta-testing` — `914498730001072149`
   - `#devs-esp-firmware` — `924989767966425158`
   - `#english-support` — `931267109726593116`
   - `#nederlandse-ondersteuning` — `815561033036333076`
3. Filter to messages posted **after** the last-checked timestamp. Discard the rest immediately — do not keep them in context.
4. Exclude messages from maintainer (user ID `384411356616720384`, username `number3nl`) and bot accounts.
5. Write current UTC timestamp to `.claude/discord_last_checked.txt`.

Keep in context only: message ID, author, channel, timestamp, text content, and attachment metadata (filename, size, URL). Discard everything else.

---

## Phase 1b: Fetch GitHub issues (O3)

1. Read `.claude/github_last_checked.txt`. Default to 7 days ago if absent.
2. Fetch issue list (body included — no separate view call yet):
   ```bash
   gh issue list --repo rvdbreemen/OTGW-firmware --state open --limit 50 \
     --json number,title,body,createdAt,updatedAt,labels,author,url
   ```
3. Filter to issues created or updated after the last-checked timestamp.
4. Exclude issues by `rvdbreemen` or accounts containing `[bot]`.
5. **Classify each issue from its body alone** (no comment fetch yet). Mark as: bug / feature / question / discussion / irrelevant.
6. **Only for issues classified as bug**: fetch comments, capped at 5:
   ```bash
   gh issue view <number> --repo rvdbreemen/OTGW-firmware \
     --json number,title,body,comments,labels,author,url \
     --jq '{number,title,body,url,author,labels,comments: .comments[:5]}'
   ```
7. Write current UTC timestamp to `.claude/github_last_checked.txt`.

---

## Phase 1c: Fetch Tweakers forum posts (O4)

1. Read `.claude/tweakers_last_checked.txt`. Default to 7 days ago if absent.
2. Fetch and sanitize RSS:
   ```bash
   mkdir -p .tmp
   curl -s --max-time 10 -A "Mozilla/5.0" \
     "https://gathering.tweakers.net/rss/list_messages/1653967" > .tmp/tweakers_rss.bin
   python3 -c "
   data = open('.tmp/tweakers_rss.bin','rb').read().decode('utf-8',errors='replace')
   open('.tmp/tweakers_rss.xml','w',encoding='utf-8').write(data)
   "
   ```
3. Parse `.tmp/tweakers_rss.xml`. For each `<item>`, extract:
   - `<dc:creator>` — author
   - `<pubDate>` — timestamp (RFC 822)
   - **`<description>` only** — short excerpt (50–200 chars, sufficient for triage)
   - `<link>` — permalink
4. Filter to items newer than last-checked timestamp. Exclude posts by `number3` or `rvdbreemen`.
5. **Bug-report keyword filter** (Dutch + English): if `<description>` contains any of `werkt niet|niet meer|fout|crash|bug|error|probleem|kapot|broken|fails|issue|doesn't work` → mark as candidate. Others → discard unless clearly actionable.
6. **Only for candidates**: fetch `<content:encoded>` from the same parsed XML for the full post body.
7. Write current UTC timestamp to `.claude/tweakers_last_checked.txt`.
8. Tweakers posts are in Dutch — summarize in English for all developer-facing output.

---

## Phase 1d: Fetch attachments — two-pass lazy load (O1)

Only process attachments for messages/issues that survive the filters above.

### Pass 1 — metadata triage (no download)

For each attachment, note: filename, size, MIME type. Apply these rules:

| Condition | Action |
|---|---|
| Size > 50 KB AND message text contains no bug keywords | Skip — note "attachment skipped (>50 KB, no bug signal in text)" |
| Size > 50 KB AND message text has bug keywords | Do Pass 2 with Grep only |
| Size ≤ 50 KB, text file (`.txt`, `.log`, `.json`) | Do Pass 2 with WebFetch first-pass |
| Image (`.png`, `.jpg`, `.webp`) | Do Pass 2 only if screenshot appears to show UI error (message text says so) |

Bug keywords (check in message text): `crash|hang|error|not working|broken|geen|werkt niet|fout|exception|reboot|disconnect|freeze`.

### Pass 2 — conditional fetch

**Text files ≤ 50 KB — WebFetch first-pass:**
```
WebFetch(url=<CDN URL>, prompt="Does this log show any errors, crashes, exceptions, or unexpected behavior? Answer yes/no and give a one-sentence summary if yes.")
```
If the answer is "no": discard. If "yes": download for Grep.

**Text files > 50 KB (bug signal confirmed) or WebFetch says "yes" — download and Grep:**
```powershell
$dir = "$env:TEMP\otgw-issues-attach"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
Invoke-WebRequest -Uri "<CDN URL>" -OutFile "$dir\<filename>" -UseBasicParsing
```
Then: `Grep(pattern="error|exception|crash|assert|panic|reboot|Exception \(\d+\)", path="C:\\Users\\rvdbr\\AppData\\Local\\Temp\\otgw-issues-attach\\<filename>", output_mode="content", -C=3)`

Never call `Read` on a full log file. Use `Grep` only.

**Images — download and Read:**
```powershell
Invoke-WebRequest -Uri "<CDN URL>" -OutFile "$dir\<filename>" -UseBasicParsing
```
Then: `Read` with the full Windows path (Claude Code can display images).

**CDN URL expiry**: Discord CDN URLs expire ~7 days after posting (signed `ex=<hex-epoch>`). On 403: re-call `read_messages` to get a fresh signed URL.

**Triage output MUST include** a one-line attachment summary for each fetched item:
`"<filename> (<size>, pass1: <webfetch verdict>): <grep findings or image description>"`

---

## Phase 2: Triage (O6)

Combine all surviving items from Phases 1–1d into a single triage list. Focus only on bug reports and actionable issues; discard the rest.

Write the triage to `.tmp/triage_result.json`:
```json
[
  {
    "id": 1,
    "source": "Discord #beta-testing",
    "reporter": "username",
    "summary": "one-line English description",
    "attachment": "filename (size): grep findings",
    "ready": true,
    "missing": ""
  }
]
```

**Present to developer** as a compact numbered list — one line per item:
```
1. [Discord #beta-testing] username — MQTT reconnect crash after router reboot (log: 3 exceptions at 20:33)
2. [GitHub #542] reporter — OT log missing data after WebSocket reconnect
3. [Tweakers] username — Thermostat setpoint not updating (needs logs)
```

Cross-reference: if the same issue appears in multiple sources, merge into one entry.

If no new issues: "No new issues since last check." Stop.

---

## Phase 2b: Backlog triage (O5)

Run once at the start of this phase:
```bash
backlog task list -l bug --plain
```

Keep the output in context. For each issue from Phase 2:

1. **Search locally** in the task list output for matching title/description keywords. Do NOT run `backlog search` per issue.
2. **If no match found**, create a task:
   ```bash
   backlog task create "Fix: <short description>" \
     -d "<what, who reported it, where>" \
     --ac "<testable criterion>" \
     -l "bug,needs-info" --priority medium
   backlog task edit <id> --ref "<source URL or Discord #channel + username + date>"
   ```
3. **If match found**, update the existing task with new information:
   ```bash
   backlog task edit <id> --append-notes "<date>: New report from <source>. <what's new>"
   ```
4. **Assess readiness**: ready = reproducible description + code-area context + at least one reporter who can validate. If not ready: keep `needs-info`, note what's missing. If ready: remove `needs-info` label.
5. **Cross-reference open `needs-info` tasks** against new messages — if missing info has arrived, update and re-assess.

**CHECKPOINT: Ask the developer which issue(s) to work on. Do not proceed until they select one.**

---

## Phase 3: Analyze the selected issue

1. Read all related messages in the thread for full context (now is the time to fetch more detail if needed).
2. Search the codebase for relevant code paths.
3. Draft a solution:
   - Root cause analysis
   - Proposed fix (files + changes)
   - Risks and side effects
   - Testing approach
4. Present the plan.

**CHECKPOINT: Wait for developer approval. Do not write code until approved.**

---

## Phase 4: Create branch and bump version

1. Verify clean working tree (`git status`). Warn and stop if dirty.
2. Derive kebab-case description (max 5 words, e.g. `mqtt-reconnect-crash`).
3. Create branch from dev:
   ```bash
   git checkout dev && git pull && git checkout -b fix-issue-<description>
   ```
4. Bump patch version in `src/OTGW-firmware/version.h` (increment `_VERSION_PATCH`; update `_SEMVER_*` and `_VERSION` consistently; leave `_VERSION_BUILD`, `_VERSION_GITHASH`, `_VERSION_DATE`, `_VERSION_TIME` for the build system).
5. Refresh derived fields:
   ```bash
   python scripts/autoinc-semver.py src/OTGW-firmware --filename version.h --update-all --increment-build 0
   ```
6. Commit version bump:
   ```bash
   git add src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash
   git commit -m "chore: bump version to vX.Y.Z-beta for fix-issue-<description>"
   ```

---

## Phase 5: Implement the fix

Follow the approved plan. Coding rules (from CLAUDE.md):
- PROGMEM for all string literals: `F()`, `PSTR()`, `snprintf_P`
- No `String` class in hot paths — use `char[]` with `strlcpy`, `snprintf_P`
- Never write to `Serial` — use `DebugTln()`, `DebugTf()`
- OTGW commands via `addOTWGcmdtoqueue()` only
- Feed watchdog in long loops: `feedWatchDog()`

Verify:
```bash
python build.py --firmware
python evaluate.py --quick
```

Fix any errors before proceeding.

---

## Phase 6: Report results

1. Commit changes:
   - GitHub-sourced: include `Fixes #<N>` or `Refs #<N>` in commit body
   - Discord-only: plain description
2. Present summary: issue source, files changed, build/eval status, branch name, follow-ups.
3. **Do NOT push** — developer reviews first.

---

## Important rules

- Never skip a checkpoint
- Never push without explicit developer permission
- Never force-push or use destructive git operations
- All code must follow OTGW-firmware coding rules (PROGMEM, no String, etc.)
- All release text in English (international audience)
- Strip Discord username 4-digit suffixes for display
- Reference GitHub issue numbers in commit messages (`Fixes #NNN`)
- Tweakers posts are Dutch — always summarize in English
- Tweakers maintainer username is `rvdbreemen` — exclude from new issue detection
