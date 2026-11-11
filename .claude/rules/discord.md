# Discord and alpha-tag publishing

This rule file is loaded in every session (no `paths:` frontmatter). It was split out of CLAUDE.md by TASK-1631.

Public-facing rules (English, answer first, real `<@ID>` mention, never name the OTGW32 board, Sergeant D's decisions recorded first) are in the AGENT-CORE block of CLAUDE.md. SAT questions to Sergeant D follow `.claude/rules/sat-ground-truth.md`.

## MCP servers

Two MCP servers are wired into Claude Code for this project:

- **`mcp__backlog__*`** — the Backlog.md MCP server, started locally by Claude Code (stdio). It stays wired, but agents do not use it: every task read and write goes through the `backlog` CLI (see Task Management and the AGENT-CORE block).
- **`mcp__discord-mcp__*`**: Discord I/O for `#dev-sat-mqtt` and the support channels. Runs as a **native Java process** (`saseq/discord-mcp`, Temurin JDK 17) on `http://localhost:8085/mcp`, no Docker. It runs under a supervisor (`D:\Users\Robert\Documents\GitHub\RvdB\discord-mcp-saseq\supervise-discord-mcp.ps1`: one instance, waits for the network, restarts the server when it exits or stops answering), started by the scheduled task **OTGW Discord MCP** at logon and every minute; the token comes from the Windows user environment (`DISCORD_TOKEN`, `DISCORD_GUILD_ID`). A SessionStart hook (`ensure-discord-mcp.ps1`) starts the task when port 8085 is closed. Logs: `discord-mcp-saseq\logs\`. Most-used tools: `read_messages` (params `channelId`, `count`), `send_message` (params `channelId`, `message`), `get_server_info`, `read_private_messages`, `send_private_message`. There is no separate login tool: JDA handshakes with Discord at startup. For attachment contents (logs, screenshots), see `.claude/commands/backlog_discord.md` (Phase 1b) and `.claude/commands/check_otgw_issues.md` (Phase 1d).

If Discord calls start failing with "Expected token to be set" or 401, the native process has dropped its gateway session; kill the `java -jar discord-mcp-1.0.0.jar` process: the supervisor starts a fresh one within about a minute (`discord-mcp-saseq\logs\supervisor.log`). Do not run `run-discord-mcp.ps1` by hand next to the supervisor. A running Claude session keeps working across a server restart; a session that started while the server was down needs `/mcp`. Only ONE instance may hold the bot token at a time: never run this alongside another discord bot process on the same token (gateway conflict). Full operational reference, restart procedure, channel discipline, and bot-side intent requirements: read `docs/guides/discord-mcp-server.md`.

## Alpha tags

**Every alpha tag on `dev` is published (maintainer directive, 2026-10-29), also when another session or agent built it.** Protocol:
1. The session that pushes a firmware commit (new `_VERSION_PRERELEASE`) builds esp32-combo from its pushed HEAD (or from the committed HEAD it is about to push, after the final rebase) and runs `bash scripts/discord-post-build.sh -m "TASK-NNN: <one-line change>"` from that worktree. The script refuses unless the zip githash is HEAD and `src/` is clean, so a pre-rebase build cannot be posted: rebuild after the rebase.
2. When the build slot or memory does not allow a separate build, the next posted zip covers the tag; its post and `-m` line name every covered tag.
3. The pushing session messages the coordinating 2.0.0 session with tag, commit, tasks and evidence. The coordinator posts the plain-language report in #alpha-testing (several tags may share one message) and keeps the ledger `.claude/alpha-build-ledger.local.md` in the main dev tree (gitignored).
4. Before every post the coordinator compares the ledger with `git log -G"_VERSION_PRERELEASE" origin/dev -- src/OTGW-firmware/version.h`; any tag that is neither posted nor named as covered is published with the next zip.

## Boiler and thermostat support questions

Maintainer directive, 2026-11-11: every question about whether a boiler or thermostat works with the gateway, or which OpenTherm messages it supports, is checked against the OpenTherm Gateway equipment matrix first: https://otgw.tclcode.com/matrix.cgi (boilers under `#boilers`, thermostats under `#thermostats`). This applies on Discord, GitHub, Tweakers, in task notes and in capture analyses.

- The matrix is maintained by Schelte Bron (PIC firmware author) from OTmonitor logs that gateway owners upload. Its rows are OpenTherm Data-IDs, its columns are boiler or thermostat models, and its symbols show which IDs that unit answered in the uploaded logs. Many crosses means weak OpenTherm support; a few crosses can also mean the logged thermostat simply never asked for those IDs.
- It is best effort, not a guarantee. Cite it as "the equipment matrix shows ..." with the model name as listed, and say when the model is missing or only partly covered.
- When the model is missing or has gaps, ask the user for a capture (and point them to the matrix page's own request to upload OTmonitor logs). Never claim support from the brand name alone.
- A capture from the user's own system outranks the matrix for that system; report a conflict between the two instead of picking one.
