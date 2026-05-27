# /backlog_discord — Respond to backlog commands from Discord

Monitor `#dev-sat-mqtt` for backlog-related requests, execute them via the Backlog CLI, and post results back.

## Token-efficiency rules (apply throughout all phases)

- **B1 task list**: Always add a status filter or `--limit 20`. Never call `backlog task list --plain` bare. Default to `-s "In Progress"` unless the user asked for a different status or all tasks.
- **B2 board**: Do NOT call `backlog board --plain` — it dumps everything. Instead run three targeted queries (To Do / In Progress / Done) with `--limit 5` each and compose a compact summary.
- **B3 attachments**: Only fetch attachments when the message text contains a bug keyword (`crash|error|issue|broken|doesn't work|werkt niet|fout`). Text files: WebFetch first-pass only. Images: download + Read only if the message says "screenshot of error/UI". Skip everything else.
- **B4 early-exit**: Read `.claude/discord_backlog_last_checked.txt`. If the timestamp is < 5 minutes old, print "Nothing new (checked < 5 min ago)" and stop — no API call.
- **B5 compact format**: Keep the formatted response under 1500 characters. Help text: use the short form in the rules below, not a full prose block.

## Configuration

- **Bot channel**: `#dev-sat-mqtt` — channel ID `1105556725714649128`
- **Timestamp file**: `.claude/discord_backlog_last_checked.txt`
- **Maintainer user ID to ignore**: `384411356616720384`

## Workflow

### Phase 1: Early-exit + read new messages (B4)

1. Read `.claude/discord_backlog_last_checked.txt`. If absent, default to 1 hour ago.
2. **Early-exit (B4)**: if timestamp is < 5 minutes old → print "Nothing new (checked < 5 min ago)" and stop.
3. Fetch messages: `mcp__discord-mcp__fetch_channel_history` with `channel_id="1105556725714649128"` and `limit=25`.
4. Filter to messages after the last-checked timestamp. Discard the rest immediately.
5. Ignore messages from bots and maintainer (user ID `384411356616720384`).
6. Write current UTC timestamp to `.claude/discord_backlog_last_checked.txt`.

**Tool namespace**: `mcp__discord-mcp__*`. No login step — `DISCORD_TOKEN` is injected via MCP config.

### Phase 1b: Attachments — conditional only (B3)

Only process attachments for messages that pass Phase 1 filtering **and** contain a bug keyword in the message text (`crash|error|issue|broken|doesn't work|werkt niet|fout`).

For qualifying messages:
- **Text files** (`.txt`, `.log`, `.json`): WebFetch first-pass only:
  ```
  WebFetch(url=<CDN URL>, prompt="Does this log show errors or unexpected behavior? Yes/no + one sentence.")
  ```
  If "no": skip. If "yes": note the finding in the reply.
- **Images**: download and Read only if the message explicitly says "screenshot of error/UI":
  ```powershell
  $dir = "$env:TEMP\discord-attach"
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  Invoke-WebRequest -Uri "<CDN URL>" -OutFile "$dir\<filename>" -UseBasicParsing
  ```
  Then `Read` with the full Windows path.
- **All other attachments**: skip silently.

Discord CDN URLs expire ~7 days after posting. On 403: re-call `fetch_channel_history` for a fresh URL.

### Phase 2: Identify actionable messages

A message is actionable if it asks about tasks, backlog, status, or assignments. Ignore general chat.

**Supported intents**:

| Intent | Example |
|--------|---------|
| List tasks | "list tasks", "show backlog", "tasks in progress" |
| Show task | "show task 42", "details on task 42" |
| Update status | "move task 42 to in progress", "mark task 42 done" |
| Assign | "assign task 42 to @sara" |
| Add note | "add note to task 42: started refactoring" |
| Search | "find tasks about mqtt", "search auth" |
| Board | "board", "show the board", "kanban" |
| Help | "help", "what can you do?" |

### Phase 3: Execute and respond (B1, B2)

For each actionable message:

1. Parse intent and extract parameters.
2. Execute the backlog operation:

   | Intent | Command |
   |--------|---------|
   | List tasks (default) | `backlog task list -s "In Progress" --plain` (B1) |
   | List tasks by status | `backlog task list -s "<status>" --plain` |
   | List all tasks | `backlog task list --plain --limit 20` (B1 — always cap) |
   | Show task | `backlog task <id> --plain` |
   | Update status | `backlog task edit <id> -s "New Status"` |
   | Assign | `backlog task edit <id> -a @name` |
   | Add note | `backlog task edit <id> --append-notes "note"` |
   | Search | `backlog search "query" --plain` |
   | Board (B2) | Three queries: `backlog task list -s "To Do" --plain --limit 5`, `-s "In Progress" --limit 5`, `-s "Done" --limit 5` |
   | Help | No command — use the compact help text below |

3. Format and post the response (see formatting guidelines below).
4. Post via `mcp__discord-mcp__discord_post_message` with `channel_id="1105556725714649128"`.

### Phase 4: Conversational follow-ups

If a message is a reply in a thread where the bot posted task details, treat it as a contextual update using the task ID from context:

- "mark AC 1 done" → `backlog task edit <id> --check-ac 1`
- "assign this to @dev" → `backlog task edit <id> -a @dev`
- "add a note: fixed the bug" → `backlog task edit <id> --append-notes "fixed the bug"`
- "what are the open ACs?" → re-fetch and show unchecked ACs only

## Response formatting guidelines (B5)

Keep all responses under 1500 characters. Use Discord markdown.

**Task list:**
```
**Tasks — In Progress**
- **#7** Setup MQTT reconnect (`In Progress`, @rob)
- **#12** Add REST endpoint (`In Progress`, @sara)
_2 tasks. "show task <id>" for details._
```

**Task detail:**
```
**Task #7 — Setup MQTT reconnect**
Status: In Progress | Assignee: @rob | Priority: high
Implement MQTT reconnect with exponential backoff.
AC: [ ] #1 Reconnect in 30s  [x] #2 Backoff  [ ] #3 Log attempts
```

**Board (B2 — three-column compact):**
```
**Board**
To Do (2): #3 Refactor settings, #9 Add OTA check
In Progress (2): #7 MQTT reconnect (@rob), #12 REST endpoint (@sara)
Done (last 5): #1, #4, #6, #8, #11
```

**Update confirmation:**
```
Done — Task #7 → **In Progress**.
```

**Help (compact):**
```
**Backlog Bot**
list tasks | list tasks in progress | show task 7 | board | search mqtt
move task 7 to in progress | mark task 7 done | assign task 7 to @rob
add note to task 7: your note here
In a thread: mark AC 1 done | what are the open ACs?
```

## Important rules

- **Never modify tasks without explicit user request**
- **Always use the backlog CLI** — never edit task files directly
- **If ambiguous**, ask for clarification in Discord rather than guessing
- **If CLI returns an error**, post a friendly message ("Task 99 not found — use `list tasks` to see available tasks")
- **Skip non-backlog messages** — do not respond to general chat
