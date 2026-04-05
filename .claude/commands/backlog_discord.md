# /backlog_discord — Respond to backlog commands from Discord

Monitor a Discord channel for backlog-related requests, execute them via the Backlog MCP, and post results back to Discord.

## Configuration

- **Bot channel**: `#devs-esp-firmware` — channel ID `924989767966425158`
- **Timestamp file**: `.claude/discord_backlog_last_checked.txt`
- **Bot user ID to ignore**: `384411356616720384` (maintainer, not the bot itself — adjust if needed)

## Workflow

### Phase 1: Connect and read new messages

1. **Login to Discord** using `mcp__discord__discord_login`.
2. **Read the last-checked timestamp** from `.claude/discord_backlog_last_checked.txt`. If the file does not exist, default to the last 1 hour.
3. **Read messages** from the bot channel using `mcp__discord__discord_read_messages` with channel ID `924989767966425158` (limit 30).
4. **Filter** to messages posted after the last-checked timestamp.
5. **Ignore** messages sent by bots (including yourself).
6. **Save the current timestamp** to `.claude/discord_backlog_last_checked.txt`.

### Phase 2: Identify actionable messages

Scan each new message for backlog-related intent. A message is actionable if it:

- Mentions the bot AND asks about tasks, backlog, status, assignments, etc.
- Contains an explicit command pattern (see below)
- Is a follow-up reply in a thread where the bot previously responded about a task

**Supported intents** (match flexibly — these are examples, not exact strings):

| Intent | Example messages |
|--------|-----------------|
| List tasks | "list tasks", "what's on the board?", "show backlog", "tasks in progress" |
| Show task | "show task 42", "details on task 42", "what's task 42 about?" |
| Task status | "status of task 42", "is task 42 done?" |
| Update status | "move task 42 to in progress", "mark task 42 done" |
| Assign task | "assign task 42 to @sara" |
| Add note | "add note to task 42: started refactoring" |
| Search | "find tasks about mqtt", "search auth" |
| Board summary | "board", "show the board", "kanban" |

If a message is not backlog-related, skip it entirely — do not respond.

### Phase 3: Execute and respond

For each actionable message, do the following:

1. **Parse the intent** and extract parameters (task ID, status, search query, etc.)
2. **Execute the corresponding backlog operation**:

   | Intent | Backlog command |
   |--------|----------------|
   | List tasks | `backlog task list --plain` (optionally with `-s "Status"`) |
   | Show task | `backlog task <id> --plain` |
   | Update status | `backlog task edit <id> -s "New Status"` |
   | Assign | `backlog task edit <id> -a @name` |
   | Add note | `backlog task edit <id> --append-notes "note text"` |
   | Search | `backlog search "query" --plain` |
   | Board summary | `backlog board --plain` |

3. **Format the response for Discord**. Keep it readable:
   - Use Discord markdown (bold, code blocks, bullet lists)
   - For task lists: show ID, title, status, assignee — one line per task
   - For task details: show title, status, assignee, description, and acceptance criteria
   - For board view: group tasks by status column
   - Keep responses under 1900 characters (Discord limit is 2000). If longer, summarize and offer to show more.

4. **Post the response** to the same channel using `mcp__discord__discord_send_message`.

### Phase 4: Handle conversational follow-ups

If a message is a **reply in a thread** where the bot previously posted task details, treat it as a contextual update:

- "mark AC 1 done" → `backlog task edit <id from context> --check-ac 1`
- "assign this to @dev" → `backlog task edit <id from context> -a @dev`
- "add a note: fixed the bug" → `backlog task edit <id from context> --append-notes "fixed the bug"`
- "what are the open ACs?" → re-fetch and show unchecked acceptance criteria

Use the task ID from the earlier message in the thread for context.

## Response formatting guidelines

### Task list response
```
**Backlog Tasks** (In Progress)

- **#7** Setup MQTT reconnect — `In Progress` (@rob)
- **#12** Add REST endpoint for sensors — `In Progress` (@sara)
- **#15** Fix watchdog timeout — `In Progress` (unassigned)

_3 tasks shown. Say "show task <id>" for details._
```

### Task detail response
```
**Task #7 — Setup MQTT reconnect**
**Status:** In Progress | **Assignee:** @rob | **Priority:** high

**Description:**
Implement automatic MQTT reconnection with exponential backoff.

**Acceptance Criteria:**
- [ ] #1 Reconnect within 30s of disconnect
- [x] #2 Exponential backoff (1s, 2s, 4s, max 60s)
- [ ] #3 Log reconnection attempts via DebugTln
```

### Update confirmation
```
Done — Task #7 status changed to **Done**.
```

## Important rules

- **Never modify tasks without explicit user request** — read operations are safe, write operations need clear intent
- **Be concise** — Discord is chat, not a document viewer
- **Respect the backlog CLI** — always use CLI commands, never edit task files directly
- **If a command is ambiguous**, ask for clarification in the Discord response rather than guessing
- **If backlog CLI returns an error**, post a friendly error message (e.g. "Task 99 not found. Use `list tasks` to see available tasks.")
- **Skip non-backlog messages entirely** — don't respond to general chat
