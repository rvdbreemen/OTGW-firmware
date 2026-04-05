# Discord Backlog Bridge -- Setup & Operations Guide

Handoff document for any Claude Code instance that needs to operate the Discord-Backlog bridge for the OTGW-firmware project.

## What This Is

A Claude Code-powered bot that monitors the `#dev-sat-mqtt` Discord channel for:
1. **Backlog commands** -- task queries and updates
2. **@bot mentions** -- conversational interactions about the project
3. **Task feedback** -- suggestions from channel participants that improve task definitions

No custom bot code. It runs entirely through Claude Code with two MCP servers.

## Architecture

```
Discord #dev-sat-mqtt (channel 1105556725714649128)
    |  (mcp-discord MCP server)
Claude Code session
    |  (backlog MCP server)
Backlog.md task files (backlog/tasks/)
```

## Prerequisites

### 1. Node.js packages

```bash
npm install -g mcp-discord
npm install -g backlog-md
```

### 2. Discord Bot Token

Environment variable `DISCORD_TOKEN` must be set. The bot is **OTGW bot#0128** (user ID `1487467924351357049`).

Required bot permissions: Read Messages, Send Messages, Read Message History.
Required privileged intents: Message Content, Server Members, Presence.

### 3. MCP Server Configuration

Configured in `.mcp.json` (project root, gitignored):

```json
{
  "mcpServers": {
    "discord": {
      "command": "npx",
      "args": ["mcp-discord"],
      "env": {
        "DISCORD_TOKEN": "${DISCORD_TOKEN}"
      }
    }
  }
}
```

The backlog MCP server is configured separately.

## How to Run

### One-shot
```
/backlog_discord
```

### Continuous polling
```
/loop 2m /backlog_discord
```

## Channel Restriction

**The bot ONLY operates in `#dev-sat-mqtt` (channel ID `1105556725714649128`).** No other channel on the OTGW Discord server (guild `812969634638725140`) is monitored or responded to. This restriction can only be changed by the project owner.

## Key IDs

| Entity | ID |
|--------|-----|
| Discord server (guild) | `812969634638725140` |
| `#dev-sat-mqtt` channel | `1105556725714649128` |
| Bot user (OTGW bot#0128) | `1487467924351357049` |
| Project owner (number3nl) | `384411356616720384` |

## Interaction Modes

### Backlog Commands
Users type task commands in plain language. The bot executes via backlog CLI and posts results. The bot also accepts task feedback and may update task metadata (descriptions, ACs, notes) based on channel discussion. Never implements code.

### @Bot Conversation
When @mentioned, the bot participates conversationally. It answers questions, shares project knowledge, discusses approaches. An @mention is never interpreted as a command to execute.

### What the bot does NOT do
- Write or modify code
- Implement features
- Make architectural decisions
- Respond in channels other than `#dev-sat-mqtt`
- Execute actions from @mentions (only from explicit commands)

## File Layout

```
.claude/
  commands/
    backlog_discord.md              # The slash command (full workflow spec)
    check_discord_issues.md         # Separate: monitors for bug reports
  docs/
    discord-backlog-bridge.md       # This document
  discord_backlog_last_checked.txt  # Timestamp tracker (auto-managed)
.mcp.json                          # MCP server config (gitignored)
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Bot doesn't respond | Run `/backlog_discord` manually or check `/loop` is running |
| "Not logged in" errors | Discord token may be expired. Set new token via `setx DISCORD_TOKEN "new-token"` |
| Login timeout | Check that Privileged Gateway Intents are enabled in Discord Developer Portal |
| Bot responds to old messages | Delete `.claude/discord_backlog_last_checked.txt` and restart |
| Backlog commands fail | Verify `backlog` CLI works: `backlog task 1 --plain` |
| Response too long | Bot auto-summarizes at 1900 chars |
