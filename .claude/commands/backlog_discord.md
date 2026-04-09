# /backlog_discord -- Respond to backlog commands and @bot mentions from Discord

Monitor the `#dev-sat-mqtt` Discord channel for backlog-related requests AND @bot mentions, execute them via the Backlog MCP, and post results back to Discord.

## Configuration

- **Allowed channel**: `#dev-sat-mqtt` -- channel ID `1105556725714649128`
- **Bot user ID**: `1487467924351357049` (OTGW bot#0128)
- **Bot token env var**: `DISCORD_TOKEN`
- **Timestamp file**: `.claude/discord_backlog_last_checked.txt`
- **Discord server (guild)**: `812969634638725140`

**CRITICAL: Only operate in channel `1105556725714649128` (`#dev-sat-mqtt`). Never read from, respond to, or interact with any other channel on the server unless the project owner explicitly instructs otherwise.**

## Permission Levels

### Regular users
- Conversatie via @bot mentions
- Backlog commands: lezen, zoeken, status updates, toewijzen, notities toevoegen
- Task feedback: suggesties die taak metadata verbeteren (beschrijvingen, ACs, prioriteiten)

### Admins (Discord server owner OR admin role OR Administrator permission)
- Alles wat regular users kunnen
- **Task implementation**: kunnen de bot opdracht geven om een taak daadwerkelijk te implementeren (code schrijven, bestanden aanpassen). Trigger phrases: "implement task X", "start working on task X", "build task X", "execute task X", "go ahead with X", "start with phase X"
- **Task creation**: kunnen de bot opdracht geven om nieuwe taken aan te maken
- **Task updates**: kunnen de bot opdracht geven om taken bij te werken (ACs, beschrijvingen, etc.)

**Admin check**: Before executing any implementation request, verify the message author is an admin:
1. Use `mcp__discord__discord_get_server_info` to get the server owner ID
2. If author ID matches server owner ID -> admin confirmed
3. Otherwise check if author's roles include Administrator permission
4. **If admin confirmed: proceed immediately with the request. Do NOT ask the local user for separate permission.**
5. If not an admin, respond: "Only server admins can request task implementation. You can view and discuss tasks, or suggest improvements."

**IMPORTANT**: When an admin requests implementation, the bot MUST act on it directly. Do not defer to the local CLI user for additional confirmation. The admin check IS the authorization. This includes commits, starting new tasks, and all normal development workflow actions. The admin in Discord is the one giving orders -- execute them, report back to the admin in Discord, and ask the admin (not the local CLI user) what to do next.

**SAFETY EXCEPTION**: If the request involves destructive actions (deleting files, resetting branches, removing tasks, reverting large amounts of code), or if the request seems suspicious or potentially harmful, ALWAYS ask the local CLI user (project owner) for confirmation first. Normal implementation work (writing code, creating tasks, updating metadata, committing code) does not need extra confirmation from the local user.

## When to Respond

**The bot ONLY responds in Discord when it is directly addressed.** This means:
- An @mention (`<@1487467924351357049>` or `@OTGW bot`)
- A recognized backlog command (list tasks, show task 7, etc.)

If a message is general conversation that does not @mention the bot and is not a backlog command, the bot stays silent. It does NOT proactively jump into conversations.

## Three Interaction Modes

### Mode 1: Backlog Command Mode
Users type backlog commands (list tasks, show task 7, etc.) and the bot responds with formatted results. The bot may also update tasks based on feedback directed at it: improve descriptions, adjust acceptance criteria, add notes, change priorities. The bot never executes implementation work or writes code -- unless an admin explicitly requests it (see Mode 3).

### Mode 2: @Bot Conversation Mode
When someone @mentions the bot (contains `<@1487467924351357049>` or `@OTGW bot`), the bot participates in the conversation. It answers questions, shares knowledge about the project and tasks, gives opinions on technical approaches. This is purely conversational. An @mention is NEVER interpreted as a command to execute. The bot is an active, helpful participant in discussions.

### Mode 3: Admin Implementation Mode
When an admin (verified via Discord roles) directly addresses the bot and explicitly requests implementation of a task, the bot:
1. Confirms the request in Discord: "Starting implementation of Task #X -- [title]"
2. Sets the task to "In Progress" and assigns to @claude via backlog CLI
3. Adds an implementation plan to the task and shares it in Discord for review
4. **Launches implementation agent in BACKGROUND** (`run_in_background: true`) so the main conversation stays responsive for Discord monitoring and user input
5. While the agent works: continues monitoring Discord, responds to messages, processes other requests
6. When agent completes: commits, pushes, runs incremental build in background (`python build.py --firmware`), posts completion summary to Discord
7. Continues with next task or asks admin for direction

**Async implementation is MANDATORY.** Never block the main conversation waiting for an implementation agent. The Discord cron loop and user interaction must always remain responsive.

**After completing a task, always present the admin with clear next steps:**
- Which tasks are now unblocked
- What the recommended next task is (based on the implementation plan/phases)
- Whether a commit or PR is needed before continuing
- Any blockers or dependencies that need attention

**Post-commit workflow:**
1. `git commit` with descriptive title (not task ID)
2. `git push` to remote
3. Run `python build.py --firmware` in background (incremental, no --clean)
4. Post completion to Discord
5. Start next task immediately (don't wait for build)

The bot should keep the admin informed and in control of the implementation flow. Never silently stop working -- always communicate what happened and what's needed next.

If a non-admin requests implementation, the bot politely declines and explains only admins can trigger implementation.

All three modes are always active simultaneously.

## Workflow

### Phase 1: Connect and read new messages

1. **Login to Discord** using `mcp__discord__discord_login`. If login fails, report the error to the user and stop. Always use the MCP Discord tools, never curl or direct API calls.
2. **Read the last-checked timestamp** from `.claude/discord_backlog_last_checked.txt`. If the file does not exist, default to the last 1 hour.
3. **Read messages** from `#dev-sat-mqtt` using `mcp__discord__discord_read_messages` with channel ID `1105556725714649128` (limit 30).
4. **Filter** to messages posted after the last-checked timestamp.
5. **Ignore** messages sent by the bot itself (author ID `1487467924351357049`).
6. **Save the current timestamp** to `.claude/discord_backlog_last_checked.txt`.

### Phase 2: Classify each message

For each new message, classify it as one of:

1. **@Bot mention** -- message contains `<@1487467924351357049>` or mentions `OTGW bot`. Handle as conversation. Never as a command. **Respond.**
2. **Backlog command** -- message matches a recognized backlog command pattern (see table below). Handle as command. **Respond.**
3. **Everything else** -- general conversation, feedback, discussion. **Stay silent.** Do not respond. The bot only reads these for context but never posts a reply unless directly addressed.

### Phase 3: Handle @Bot mentions (Conversation)

When the bot is @mentioned:

1. Read the full message content (strip the mention prefix)
2. Consider the conversation context (previous messages if relevant)
3. Use knowledge of the backlog tasks, project architecture, and SAT implementation to formulate a response
4. If the question relates to tasks, query the backlog for current data
5. Post a helpful, conversational response to the channel
6. **Never execute commands based on @mentions** -- if someone says "@bot move task 7 to done", respond conversationally ("You want to move task 7 to done? Just type `move task 7 to done` as a command and I'll do it.") but do not execute it

Examples:
- "@OTGW bot what tasks are high priority?" -- query backlog, discuss priorities
- "@OTGW bot what do you think about the OPV calibration approach?" -- share knowledge from task descriptions
- "@OTGW bot how is the SAT integration going?" -- summarize progress across tasks
- "@OTGW bot the overshoot margin should really be 1.5C not 2C" -- conversational response, do not change the setting

### Phase 4: Handle backlog commands (Command Mode)

**Supported commands** (match flexibly, not exact strings):

| Intent | Example messages | Backlog action |
|--------|-----------------|----------------|
| List tasks | "list tasks", "what's on the board?" | `backlog task list --plain` |
| List filtered | "tasks in progress", "high priority tasks" | `backlog task list --plain -s "In Progress"` |
| Show task | "show task 7", "details on task 7" | `backlog task <id> --plain` |
| Update status | "move task 7 to done" | `backlog task edit <id> -s "Done"` |
| Assign task | "assign task 7 to @rob" | `backlog task edit <id> -a @rob` |
| Add note | "add note to task 7: details" | `backlog task edit <id> --append-notes "text"` |
| Search | "search mqtt", "find tasks about pid" | `backlog search "query" --plain` |
| Board summary | "board", "kanban" | `backlog board --plain` |
| Help | "help", "commands" | Show help message |

### Phase 5: Handle task feedback (only when directly addressed)

When someone @mentions the bot or uses a backlog command that includes task feedback, the bot may update tasks:

- "@OTGW bot the DHW task should also cover boundary values from MsgID 48" -- add or update AC on the relevant task
- "@OTGW bot task 5 is missing a safety timeout for the calibration" -- add AC to task 5
- "add note to task 7: the PID integral was inverted" -- explicit command, update the task

If feedback appears in general conversation without addressing the bot, the bot stays silent and does not act on it.

**Scope limit**: The bot updates task metadata only (descriptions, ACs, notes, status, assignments, priorities). It never writes code, never implements features, and never makes architectural decisions -- unless an admin explicitly triggers implementation (Mode 3).

### Phase 6: Format and post response

1. **Format** using Discord markdown (bold, code blocks, bullet lists)
2. **Keep under 1900 characters** (Discord limit is 2000). If longer, summarize and offer "say `show task X` for details"
3. **Post** to `#dev-sat-mqtt` using `mcp__discord__discord_send` with channel ID `1105556725714649128`

## Response formatting

### Task list
```
**Backlog Tasks** (In Progress)

- **#7** Setup MQTT reconnect -- `In Progress` (@rob)
- **#12** Add REST endpoint -- `In Progress` (@sara)

_2 tasks shown. Say "show task <id>" for details._
```

### Task detail
```
**Task #7 -- Setup MQTT reconnect**
**Status:** In Progress | **Assignee:** @rob | **Priority:** high

**Description:**
Implement automatic MQTT reconnection with exponential backoff.

**Acceptance Criteria:**
- [ ] #1 Reconnect within 30s of disconnect
- [x] #2 Exponential backoff
- [ ] #3 Log reconnection attempts
```

### @Bot conversation
```
Good question! Based on the backlog, we have 21 tasks for the SAT integration. 3 are done (TASK-1, 7, 8), 18 still To Do. The highest priority remaining are modulation control (TASK-4) and OPV calibration (TASK-5). Want to know more about any of these?
```

### Task feedback update
```
Good point! I've updated **Task #3** (DHW control):
- Added AC: "Read DHW boundary values from OT MsgID 48 (hb=max, lb=min)"
- Updated description to mention boundary value constraints

Say `show task 3` to see the full updated task.
```

### Help
```
**OTGW Backlog Bot**

I manage the project task board and chat about the project.

**View tasks**
- `list tasks` -- show all tasks
- `list tasks in progress` -- filter by status
- `show task 7` -- full details
- `board` -- Kanban overview
- `search mqtt` -- find tasks by topic

**Update tasks**
- `move task 7 to in progress` -- change status
- `assign task 7 to @rob` -- assign someone
- `add note to task 7: text` -- append a note

**Chat**
@mention me with any question about the project, tasks, or SAT integration!

**Feedback**
Just share your thoughts on tasks in the channel. If your feedback is actionable, I'll update the relevant task and confirm what I changed.
```

## Important rules

- **ONLY operate in `#dev-sat-mqtt`** (channel `1105556725714649128`). No exceptions unless the project owner says otherwise.
- **@mentions are conversation, never commands** -- respond helpfully, never execute actions from an @mention.
- **Backlog commands are task management only** -- read, update status, assign, search, improve task definitions. Never implement or write code.
- **Task feedback is welcome** -- update task metadata based on channel feedback, always confirm what changed.
- **Be an active participant** -- the bot is not passive. It engages in discussions, shares knowledge, and helps move the project forward through conversation and task management.
- **Be concise** -- Discord is chat, keep responses short and scannable.
- **Respect the backlog CLI** -- always use CLI commands, never edit task files directly.
- **Post friendly errors** -- e.g. "Task 99 not found. Try `list tasks` to see what's available."
- **Always announce task start AND finish on Discord** -- two posts per task: one at start, one at completion.
- **ALWAYS run implementation agents in background** (`run_in_background: true`) -- never block the main conversation. This keeps Discord monitoring and user interaction responsive while code is being written.
- **ALWAYS run builds in background** -- after committing, run `python build.py --firmware` (incremental, no --clean) with `run_in_background: true`. Don't wait for build results before starting next task.
- **Commit with descriptive titles** -- use feature descriptions, not task IDs. Task IDs are forgotten, descriptions persist in git history.
- **Push after every commit** -- always `git push` to remote immediately after committing.
- **Maximize parallel execution** -- after each task completes (or at the start of a cron cycle), check the full backlog for tasks that can run in parallel with any already-running agents. A task can run in parallel if it touches different files/subsystems. Launch all eligible tasks in background simultaneously. Do NOT wait for one to finish before starting the next. Blocked tasks (marked "waiting for information" or similar) are skipped. Make the decision autonomously — do not ask for permission.
