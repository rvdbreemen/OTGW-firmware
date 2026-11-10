# Discord MCP server and the Discord bot: operations guide

How Claude Code sessions read and post on the OTGW Discord server. `.claude/rules/discord.md` (section "MCP
servers") points here. Replaces `.claude/docs/discord-backlog-bridge.md`, which described an
earlier setup (npx `mcp-discord`, the backlog MCP server, one channel) and was removed in
TASK-1625.

## What runs where

| Part | What it is |
|---|---|
| Discord MCP server | Native Java process `saseq/discord-mcp` (Temurin JDK 17) on Robert's workstation, `http://localhost:8085/mcp`. No Docker since 2026-07-04. |
| Supervisor | `D:\Users\Robert\Documents\GitHub\RvdB\discord-mcp-saseq\supervise-discord-mcp.ps1` (since 2026-11-10). One instance (named mutex), waits until `discord.com:443` is reachable, adopts a server that already runs, starts `java -jar discord-mcp-1.0.0.jar` with `DISCORD_TOKEN` and `DISCORD_GUILD_ID` from the Windows user environment and `SPRING_PROFILES_ACTIVE=http`, checks port 8085 every 30 s and restarts the server when it exits or fails 4 checks (back-off 15 to 120 s). Logs in `discord-mcp-saseq\logs\` (`supervisor.log`, `server-<stamp>.log`). |
| Autostart | Scheduled task **OTGW Discord MCP** (user, no admin): at logon after 20 s and every minute; a running supervisor makes the extra starts no-ops. Launched windowless through `start-supervisor-hidden.vbs`. The old Startup-folder shortcut is parked as `discord-mcp-saseq.lnk.bak`: do not restore it. |
| Claude Code hook | `ensure-discord-mcp.ps1`, a SessionStart hook in the user `settings.json`: when port 8085 is closed it starts the task and waits up to 40 s. |
| Why | The server logs in to Discord (JDA) before it opens port 8085. Started straight at logon without network, the login failed, the JVM exited and nothing restarted it; the old launcher logged nothing. |
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

1. Kill the `java -jar discord-mcp-1.0.0.jar` process. The supervisor starts a fresh one
   (about 15 s back-off plus about 18 s start-up); `logs\supervisor.log` shows it. Do not run
   `run-discord-mcp.ps1` by hand next to the supervisor: two processes on one token fight
   over the gateway.
2. A Claude session that was already connected keeps working (tested 2026-11-10). A session
   that started while the server was down needs `/mcp` once.
3. To stop the server on purpose: `Disable-ScheduledTask 'OTGW Discord MCP'`, then stop the
   supervisor `powershell` process and the `java` process; `Enable-ScheduledTask` to resume.

## Bot-side settings

Carried over from the removed bridge document and not re-checked since: the bot needs the
permissions Read Messages, Send Messages and Read Message History, and the privileged gateway
intents Message Content, Server Members and Presence in the Discord Developer Portal.
