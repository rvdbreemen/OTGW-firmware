# Maintainer directive changelog

Dated maintainer directives that shaped the agent instructions, where each one lives now, and which earlier
directive it replaced. Started by TASK-1631 (2026-11-05), when CLAUDE.md was split into a core plus
`.claude/rules/` and `docs/guides/`. The running instruction text states only the rule in force; the history
lives here.

## Directives in force

| Date | Directive | Lives in |
|---|---|---|
| 2026-06-19 | One line, no stray sub-branches; delete a parallel lane right after merging | `docs/guides/worktrees.md`, CLAUDE.md summary |
| 2026-06-20 | Branch model: `dev` = 2.x default, `otgw-1.x.x` = 1.x, `main` = latest public release | AGENT-CORE block |
| 2026-07-05 | Build and flash only through the wrappers, never `python build.py` / `flash_esp.py` | CLAUDE.md "Build, flash and bench", `docs/guides/agent-build-and-bench.md` |
| 2026-10-14 | Reuse open source before building your own | CLAUDE.md "Design Principles" |
| 2026-10-20 | `/implement-next-task` replaces the plan wait with an advisor self-review | CLAUDE.md "Tasks" |
| 2026-10-22 | SAT ground truth and anti-drift rules | `.claude/rules/sat-ground-truth.md` |
| 2026-10-23 | Record every Sergeant D decision first; check SAT decisions against the register | `.claude/rules/sat-ground-truth.md` |
| 2026-10-24 | Resolve unclear SAT behaviour by questioning Sergeant D step by step | `.claude/rules/sat-ground-truth.md` |
| 2026-10-24 | Flash over WiFi OTA first, USB only with `--migrate --host` | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-10-25 | Pace SAT questions to Sergeant D's presence | `.claude/rules/sat-ground-truth.md` |
| 2026-10-29 | One build at a time machine-wide, max 4 jobs, esp32-combo only on `dev` | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-10-29 | Every alpha tag on `dev` is published, also when another session built it | `.claude/rules/discord.md` |
| 2026-10-29 | Open questions go to Robert interactively with a weighed recommendation | CLAUDE.md "Design Principles" |
| 2026-10-30 | Per-task cadence: bench-test every firmware task before its zip is published | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-11-01 | `release/2.x` is the stable 2.x line (TASK-1545) | CLAUDE.md "Git push policy" |

## Replaced directives

| Date | Old directive | Replaced by | Date replaced |
|---|---|---|---|
| before 2026-10-25 | Repeat an unanswered SAT question to Sergeant D every 12 hours | Pace questions to his presence: check hourly, re-ask as soon as he is back | 2026-10-25 |
| 2026-10-29 | Batch cadence: build and publish after every 4 finished tasks | Per-task cadence: each task bench-tested before its zip is published | 2026-10-30 |
| TASK-1014 | Flat single-flight rule for web UI API calls (one request in flight) | ADR-165: at most 2 in flight (TASK-1015 load study) | ADR-165 |
| 2026-06-20 | `feature-2.0.0-esp32s3-async`, `feature-2.0.0-esp32s3-only`, `feature-dev-2.0.0-otgw32-esp32-sat-support` as work lines | Folded into `dev`; historical, not active | 2026-06-20 |
| before 2026-07-04 | Discord MCP in a Docker container plus the `gustycube` Node server | Native Java `saseq/discord-mcp` process, one instance per token | 2026-07-04 |
| before TASK-1160 | `flash_otgw --update --app` also wrote a littlefs image found in `build\` | `--update --app <bin>` writes the app only and keeps the settings | TASK-1160 |
| before 2026-06-02 | Platform abstraction in per-platform headers incl. an ESP8266 header | The project's own Platform library under `src/libraries/Platform/src/` (c880a0203) | 2026-06-02 |

## How to add an entry

A new maintainer directive goes into the instruction file where it applies (CLAUDE.md core, a rule or a
guide) and gets a row under "Directives in force". When it replaces an earlier one, remove the old text from the
running instructions, move its row to "Replaced directives" and name the date it was replaced.
