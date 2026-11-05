# Running the agentic workflow on a second machine

What lives outside the repository, where it lives on the main workstation, and how to set it up
again on another Windows machine. TASK-1632. The repository itself (`git clone`, then
`git submodule update --init --recursive`) is the easy part; this guide covers the rest.

## Local state outside git

| What | Where on the main workstation | In git? | How to recreate |
|---|---|---|---|
| Discord people and per-person rules | `.claude/discord-people.local.md` (main dev tree) | no, gitignored | Copy `.claude/discord-people.example.md` and fill in names, user IDs and rules |
| Alpha/beta build ledger | `.claude/alpha-build-ledger.local.md` (main dev tree) | no, gitignored | `python scripts/alpha_ledger.py --last 40` rebuilds tag, bump commit, tasks, posted and covered-by from git plus the files below |
| Published tags | `%LOCALAPPDATA%\OTGW-capture\published-tags.txt` | no | Written by `scripts/discord-post-build.sh`; on a new machine start from the tags already posted (copy the file) or the script may re-post an old tag |
| Post log | `%LOCALAPPDATA%\OTGW-capture\published-posts.tsv` | no | Written by `scripts/discord-post-build.sh` since TASK-1632 (time, tag, build, message id, -m text) |
| Discord webhook for build zips | `%LOCALAPPDATA%\OTGW-capture\discord-alpha-webhook.url` | no, secret | Create a webhook on #alpha-testing (Discord channel settings > Integrations) and put its URL in this file. Never print or commit it |
| Test secrets and bench captures | `%LOCALAPPDATA%\OTGW-capture\` | no | Recreate per test; captures stay local |
| Machine-wide build lock | `%LOCALAPPDATA%\OTGW-build\build.lock` | no | Created by `build.py`; nothing to copy |
| Private field evidence | `.claude/field-evidence.local.md` | no, gitignored | Not reconstructable; copy it if you need it |

## Tools

1. **Python 3.10 to 3.13** for the build (the espressif32 platform rejects 3.14). Build only through
   `build.bat`, which creates `.build-venv` from `requirements-build.txt` and downloads the
   PlatformIO toolchain.
2. **Node.js and the backlog CLI**: `npm install -g backlog.md`. On Windows, pass multi-line text
   through `node <npm-prefix>/node_modules/backlog.md/cli.js` instead of `backlog.cmd`, which cuts
   arguments at the first newline (TASK-1635).
3. **Git hooks**: `git config core.hooksPath .githooks` (a Claude Code SessionStart hook sets it when
   it is unset, TASK-1627).
4. **Git Bash**: Claude Code runs the project hooks through it, and `scripts/discord-post-build.sh`
   must run in Git Bash, not WSL bash (WSL has no `LOCALAPPDATA`).

## Discord MCP server

Follow `docs/guides/discord-mcp-server.md`. In short: Temurin JDK 17, the `saseq/discord-mcp` jar,
a launcher that sets `DISCORD_TOKEN`, `DISCORD_GUILD_ID` and `SPRING_PROFILES_ACTIVE=http`, port
8085, then reconnect with `/mcp` in Claude Code.

Only one process may hold the bot token. Before you start the server on the second machine, stop
it on the first.

## Bench

The bench units, their addresses and serial ports are described in the session memory of the
main workstation, not in the repository. On a second machine, find the units by MAC address in
the router's ARP table and flash over WiFi OTA first (CLAUDE.md, "Test automation").

## Check

- `python scripts/alpha_ledger.py --last 10` prints the recent tags with their posted state.
- `bash scripts/discord-post-build.sh --dry-run` runs every check without posting.
- `python evaluate.py --quick` and `python scripts/run_tests.py --no-host` run.
