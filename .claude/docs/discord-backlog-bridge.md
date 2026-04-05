# Discord ↔ Backlog.md Bridge — Setup & Operations Guide

Handoff document for any Claude Code instance that needs to operate the Discord-Backlog bridge for the OTGW-firmware project.

## What This Is

A Claude Code–powered bot that monitors a Discord channel for task-related messages, queries the Backlog.md task board, and posts responses back to Discord. No custom bot code — it runs entirely through Claude Code with two MCP servers.

## Architecture

```
Discord channel
    ↕  (mcp-discord MCP server)
Claude Code session
    ↕  (backlog MCP server)
Backlog.md task files (backlog/tasks/)
```

Claude Code acts as the glue: it reads Discord messages, interprets intent, runs backlog CLI commands, and posts formatted results back to Discord.

## Prerequisites

### 1. Node.js packages

```bash
npm install -g mcp-discord    # or use npx (already configured)
npm install -g backlog-md     # the Backlog.md CLI
```

Verify both work:
```bash
backlog --help
npx mcp-discord --help
```

### 2. Discord Bot Token

You need a Discord bot token with these permissions:
- Read Messages / View Channels
- Send Messages
- Read Message History

The token is passed via environment variable `DISCORD_TOKEN`. In the MCP config it uses a prompt input so you'll be asked on startup.

### 3. Discord Channel ID

You need the numeric channel ID for the channel the bot monitors. To get it:
1. Discord Settings → Advanced → enable **Developer Mode**
2. Right-click the target channel → **Copy Channel ID**

Currently configured channel: `#devs-esp-firmware` — `924989767966425158`
**TODO**: Update to `#dev-sat-mqtt` once the channel ID is provided.

### 4. MCP Server Configuration

The MCP servers are configured in `.claude/.vscode/mcp.json`:

```json
{
  "inputs": [
    {
      "type": "promptString",
      "id": "discord-token",
      "description": "Discord Bot Token",
      "password": true
    }
  ],
  "servers": {
    "backlog": {
      "type": "stdio",
      "command": "backlog",
      "args": ["mcp", "start"],
      "env": {
        "BACKLOG_CWD": "${workspaceFolder}"
      }
    },
    "discord": {
      "type": "stdio",
      "command": "npx",
      "args": ["mcp-discord"],
      "env": {
        "DISCORD_TOKEN": "${input:discord-token}"
      }
    }
  }
}
```

### 5. Permissions

In `.claude/settings.json`, the Discord MCP tools and Bash are pre-allowed:

```json
{
  "permissions": {
    "allow": [
      "mcp__discord__*",
      "Bash(*)"
    ]
  }
}
```

## How to Run

### One-shot (check once and respond)

```
/backlog_discord
```

### Continuous polling

```
/loop 2m /backlog_discord
```

This runs `/backlog_discord` every 2 minutes. Adjust the interval as needed (e.g., `5m` for less frequent checks).

## How It Works — Step by Step

When `/backlog_discord` runs, Claude Code does the following:

1. **Login** to Discord via `mcp__discord__discord_login`
2. **Read timestamp** from `.claude/discord_backlog_last_checked.txt` (tracks what's already been processed)
3. **Fetch messages** from the configured channel via `mcp__discord__discord_read_messages`
4. **Filter** to new messages only (after the stored timestamp), ignoring bot messages
5. **Classify** each message — is it a backlog command, help request, or unrelated chat?
6. **Execute** the matching backlog CLI command for each actionable message
7. **Format** the result as Discord-friendly markdown
8. **Post** the response back to Discord via `mcp__discord__discord_send_message`
9. **Update** the timestamp file

## Key MCP Tools

### Discord MCP (`mcp-discord`)

| Tool | Purpose |
|------|---------|
| `mcp__discord__discord_login` | Authenticate the bot |
| `mcp__discord__discord_read_messages` | Read messages from a channel (params: channel_id, limit) |
| `mcp__discord__discord_send_message` | Post a message to a channel (params: channel_id, content) |

### Backlog MCP (`backlog mcp start`)

The backlog MCP exposes the same operations as the `backlog` CLI. Key commands:

| Operation | CLI equivalent |
|-----------|---------------|
| List tasks | `backlog task list --plain` |
| Show task | `backlog task <id> --plain` |
| Edit task | `backlog task edit <id> -s "Status"` |
| Search | `backlog search "query" --plain` |
| Board view | `backlog board --plain` |

Always use `--plain` flag for AI-readable output.

## Supported Discord Commands

Users in the channel can type these (matched flexibly, not exact strings):

| What they type | What happens |
|----------------|-------------|
| `list tasks` | Shows all tasks with ID, title, status, assignee |
| `list tasks in progress` | Filtered by status |
| `show task 7` | Full task details including ACs |
| `board` | Kanban-style board summary |
| `search mqtt` | Fuzzy search across tasks |
| `move task 7 to done` | Updates task status |
| `assign task 7 to @rob` | Assigns a task |
| `add note to task 7: details here` | Appends implementation note |
| `help` | Shows the help message with all commands |

### Thread follow-ups

When the bot posts task details, users can reply in the thread with contextual commands:
- `mark AC 1 done` — checks acceptance criterion #1 on the task from the parent message
- `what are the open ACs?` — re-fetches and shows unchecked criteria
- `assign this to @sara` — assigns without repeating the task ID

## File Layout

```
.claude/
├── commands/
│   ├── backlog_discord.md          # The slash command (full workflow spec)
│   └── check_discord_issues.md     # Separate: monitors for bug reports
├── docs/
│   └── discord-backlog-bridge.md   # This document
├── discord_backlog_last_checked.txt # Timestamp tracker (auto-managed)
├── discord_last_checked.txt         # Used by check_discord_issues
├── settings.json                    # Permissions config
└── .vscode/
    └── mcp.json                     # MCP server definitions
```

## Changing the Channel

To point the bot at a different Discord channel:

1. Get the new channel ID (Developer Mode → right-click → Copy Channel ID)
2. Edit `.claude/commands/backlog_discord.md`
3. Replace the channel ID in two places:
   - Line ~7: `**Bot channel**: #channel-name — channel ID \`NEW_ID\``
   - Line ~17: `mcp__discord__discord_read_messages` with channel ID `NEW_ID`

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Bot doesn't respond | Check that `/loop` is running, or run `/backlog_discord` manually |
| "Not logged in" errors | The Discord token may have expired — restart the Claude Code session to re-enter it |
| Bot responds to old messages | Delete `.claude/discord_backlog_last_checked.txt` and restart — it will default to the last 1 hour |
| Backlog commands fail | Verify `backlog` CLI is installed: `backlog --help` |
| Messages are being skipped | The bot ignores non-backlog messages by design. Users must mention tasks, status, or use known command patterns |
| Response too long for Discord | The command limits responses to 1900 chars. If data is larger, it summarizes and offers "say show task X for details" |

## Important Rules for the Operating Instance

- **Never edit task files directly** — always use `backlog` CLI commands
- **Never modify tasks without explicit user request** — reads are safe, writes need clear intent
- **Be concise** — Discord is chat, keep responses short and scannable
- **Skip non-backlog messages** — don't respond to general conversation
- **If ambiguous**, ask for clarification in the Discord response rather than guessing
- **Post friendly errors** — e.g. "Task 99 not found. Use `list tasks` to see available tasks."
