# Discord MCP server and the Discord bot: operations guide

How Claude Code sessions read and post on the OTGW Discord server. CLAUDE.md (section "MCP
Servers") points here. Replaces `.claude/docs/discord-backlog-bridge.md`, which described an
earlier setup (npx `mcp-discord`, the backlog MCP server, one channel) and was removed in
TASK-1625.

## What runs where

| Part | What it is |
|---|---|
| Discord MCP server | Native Java process `saseq/discord-mcp` (Temurin JDK 17) on Robert's workstation, `http://localhost:8085/mcp`. No Docker since 2026-07-04. |
| Launcher | `D:\Users\Robert\Documents\GitHub\RvdB\discord-mcp-saseq\run-discord-mcp.ps1`. Reads `DISCORD_TOKEN` and `DISCORD_GUILD_ID` from the Windows user environment and sets `SPRING_PROFILES_ACTIVE=http`. |
| Autostart | Startup-folder shortcut `discord-mcp-saseq.lnk`, at logon. |
| Tool namespace | `mcp__discord-mcp__*`. No login tool: JDA connects to Discord when the process starts. |
| Bot identity | The bot posts as **BlueFlame Bot**. |
| Build zips | Not through the MCP server: `scripts/discord-post-build.sh` posts the esp32-combo zip to #alpha-testing through a webhook whose URL lives in `%LOCALAPPDATA%\OTGW-capture\discord-alpha-webhook.url` (a secret, never printed or committed). |

Only one process may hold the bot token at a time. Never run this server next to another bot
process on the same token: the gateway connections conflict.

## Most-used tools

| Tool | Parameters | Use |
|---|---|---|
| `read_messages` | `channelId`, `count` | Read the newest messages of a channel |
| `send_message` | `channelId`, `message` | Post a message |
| `get_server_info` | | Check the connection |
| `read_private_messages`, `send_private_message` | | Direct messages |

Attachments (logs, screenshots): see `.claude/commands/backlog_discord.md` (Phase 1b) and
`.claude/commands/check_otgw_issues.md` (Phase 1d).

## Channels

Guild `812969634638725140`.

| Channel | ID | How agents use it |
|---|---|---|
| #alpha-testing | `1514720723980259460` | Alpha zips and per-task reports, SAT questions to Sergeant D, tester feedback. Read every run; no backlog commands are accepted here. |
| #dev-sat-mqtt | `1105556725714649128` | Backlog commands (list, show, move, assign, note), new-feature design and SAT simulation rounds. |
| #beta-testing | `914498730001072149` | Read by `/check_otgw_issues`. |
| #devs-esp-firmware | `924989767966425158` | Read by `/check_otgw_issues`. |
| #english-support | `931267109726593116` | Read by `/check_otgw_issues`. |
| #nederlandse-ondersteuning | `815561033036333076` | Read by `/check_otgw_issues`. |

Maintainer (Robert) user ID: `384411356616720384`.

## Rules for what the bot says and does

The authoritative rules are in `.claude/commands/backlog_discord.md` and the AGENT-CORE and SAT
ground truth blocks of CLAUDE.md. In short:

- Answer in English, answer first, with the real `<@ID>` mention of the person.
- People and their per-person rules live in `.claude/discord-people.local.md` (local and
  gitignored, because per-person rules do not belong in a public repository). Without that file,
  answer only Sergeant D and Robert.
- Testers have no admin control: a change they ask for becomes a task labelled
  `needs-maintainer-ok`. Read-only members get one reply: "Changes go through Robert."
- Every SAT correction or decision by Sergeant D is recorded in
  `docs/sat/sergeant-d-corrections.md` in the same session, before it is implemented.
- Never name the OTGW32 board in public.

## When calls fail

Symptom: Discord calls fail with "Expected token to be set" or HTTP 401. The Java process has
dropped its gateway session.

1. Run `run-discord-mcp.ps1` again, or kill the stale `java -jar discord-mcp-1.0.0.jar` process
   and then run the launcher.
2. Reconnect the MCP server in Claude Code with `/mcp`.

## Bot-side settings

Carried over from the removed bridge document and not re-checked since: the bot needs the
permissions Read Messages, Send Messages and Read Message History, and the privileged gateway
intents Message Content, Server Members and Presence in the Discord Developer Portal.
