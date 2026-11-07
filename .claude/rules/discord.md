# Discord and alpha-tag publishing

This rule file is loaded in every session (no `paths:` frontmatter). It was split out of CLAUDE.md by TASK-1631.

Public-facing rules (English, answer first, real `<@ID>` mention, never name the OTGW32 board, Sergeant D's decisions recorded first) are in the AGENT-CORE block of CLAUDE.md. SAT questions to Sergeant D follow `.claude/rules/sat-ground-truth.md`.

## MCP servers

Two MCP servers are wired into Claude Code for this project:

- **`mcp__backlog__*`** — the Backlog.md MCP server, started locally by Claude Code (stdio). It stays wired, but agents do not use it: every task read and write goes through the `backlog` CLI (see Task Management and the AGENT-CORE block).
- **`mcp__discord-mcp__*`** — Discord I/O for `#dev-sat-mqtt` and the support channels. Runs as a **native Java process** (`saseq/discord-mcp`, Temurin JDK 17) on `http://localhost:8085/mcp` — no Docker. Launcher: `D:\Users\Robert\Documents\GitHub\RvdB\discord-mcp-saseq\run-discord-mcp.ps1`, which reads `DISCORD_TOKEN` + `DISCORD_GUILD_ID` from the Windows user environment (`SPRING_PROFILES_ACTIVE=http`). It auto-starts at logon via a Startup-folder shortcut (`discord-mcp-saseq.lnk`). Most-used tools: `read_messages` (params `channelId`, `count`), `send_message` (params `channelId`, `message`), `get_server_info`, `read_private_messages`, `send_private_message`. There is no separate login tool — JDA handshakes with Discord at startup. For attachment contents (logs, screenshots), see `.claude/commands/backlog_discord.md` (Phase 1b) and `.claude/commands/check_otgw_issues.md` (Phase 1d).

If Discord calls start failing with "Expected token to be set" or 401, the native process has dropped its gateway session; restart it (`schtasks`-free: run `run-discord-mcp.ps1`, or kill the stale `java -jar discord-mcp-1.0.0.jar` PID and re-run the launcher) plus an MCP reconnect (`/mcp` in Claude Code). Only ONE instance may hold the bot token at a time — never run this alongside another discord bot process on the same token (gateway conflict). Full operational reference, restart procedure, channel discipline, and bot-side intent requirements: read `docs/guides/discord-mcp-server.md`.

## Alpha tags

**Every alpha tag on `dev` is published (maintainer directive, 2026-10-29), also when another session or agent built it.** Protocol:
1. The session that pushes a firmware commit (new `_VERSION_PRERELEASE`) builds esp32-combo from its pushed HEAD (or from the committed HEAD it is about to push, after the final rebase) and runs `bash scripts/discord-post-build.sh -m "TASK-NNN: <one-line change>"` from that worktree. The script refuses unless the zip githash is HEAD and `src/` is clean, so a pre-rebase build cannot be posted: rebuild after the rebase.
2. When the build slot or memory does not allow a separate build, the next posted zip covers the tag; its post and `-m` line name every covered tag.
3. The pushing session messages the coordinating 2.0.0 session with tag, commit, tasks and evidence. The coordinator posts the plain-language report in #alpha-testing (several tags may share one message) and keeps the ledger `.claude/alpha-build-ledger.local.md` in the main dev tree (gitignored).
4. Before every post the coordinator compares the ledger with `git log -G"_VERSION_PRERELEASE" origin/dev -- src/OTGW-firmware/version.h`; any tag that is neither posted nor named as covered is published with the next zip.
