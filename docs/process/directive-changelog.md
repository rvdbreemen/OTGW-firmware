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
| 2026-11-21 | SAT ground truth: our implementation plus the recorded decisions of Sergeant D and Robert is leading; Alex Wijn's SAT (`other-projects/SAT-develop/`) is a reference; comparisons for Sergeant D are factual, with pros, cons and a recommendation, and use field data, replays of both implementations, the OpenTherm spec and the low-load application notes | `.claude/rules/sat-ground-truth.md`, AGENT-CORE block |
| 2026-11-28 | SAT reference: always compare with Alex Wijn's latest 5.x release; the snapshot `other-projects/SAT-develop/` follows that release (5.1.0 = 9c2fad9) instead of `develop`. Robert: "From now on always compare to latest SAT component from Alexwijm 5.x. Sync latest version to other project store." The ranking of ADR-292 is unchanged (register entries 536, 537) | `.claude/rules/sat-ground-truth.md`, AGENTS.md ground-truth block, `other-projects/README.md` |
| 2026-10-23 | Record every Sergeant D decision first; check SAT decisions against the register | `.claude/rules/sat-ground-truth.md` |
| 2026-10-24 | Resolve unclear SAT behaviour by questioning Sergeant D step by step | `.claude/rules/sat-ground-truth.md` |
| 2026-10-24 | Flash over WiFi OTA first, USB only with `--migrate --host` | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-10-25 | Pace SAT questions to Sergeant D's presence | `.claude/rules/sat-ground-truth.md` |
| 2026-10-29 | One build at a time machine-wide, max 4 jobs, esp32-combo only on `dev` | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-11-11 | Only the bench-tester agent flashes and bench-tests; the developer builds and pushes after its OK; the bench-tester ticks the bench ACs (TASK-1826) | AGENT-CORE block, CLAUDE.md "Build, flash and bench", `docs/guides/agent-build-and-bench.md`, the implement-next-task, beta-release-2x and release-2x skills |
| 2026-10-29 | Every alpha tag on `dev` is published, also when another session built it | `.claude/rules/discord.md` |
| 2026-10-29 | Open questions go to Robert interactively with a weighed recommendation | CLAUDE.md "Design Principles" |
| 2026-10-30 | Per-task cadence: bench-test every firmware task before its zip is published (since 2026-11-11 the bench-tester agent runs that bench test) | `docs/guides/agent-build-and-bench.md`, CLAUDE.md summary |
| 2026-11-01 | `release/2.x` is the stable 2.x line (TASK-1545) | CLAUDE.md "Git push policy" |
| 2026-11-14 | Standing permission for 2.x public betas: the coordinator invokes `/beta-release-2x` itself in its publish window (since 2026-11-27: at most one public beta per 3 hours, 07:00 to 22:00); stable releases keep the per-action yes (TASK-1948) | AGENT-CORE block (hard-stop exception), `.claude/skills/beta-release-2x/SKILL.md` (model invocation allowed) |
| 2026-11-16 | 2.0.0 stable gate = TASK-1740 (entry-386 criterion, target Sat 21 Nov if the Wed-Fri field runs are clean); the SAT low-load epic TASK-1412 and its full gate move to 2.1.0, 2.0.0 keeps only TASK-1412.24 (field-run request), TASK-1412.25 (the Wed-Fri runs) and the host LL contract (Robert, DM 1571910380756668488) | TASK-1740, TASK-1412 |
| 2026-11-20 | Explain the number, and sequence verifiable units (from the pstack principles) | CLAUDE.md and AGENTS.md "Design Principles" |
| 2026-11-27 | Public betas: at most one per 3 hours, published between 07:00 and 22:00 (T = publish time); everything else of the flow stays (docs-only release commit at T-75, 60 min soak on the exact binary, one cause per beta). Robert: "Je mag publieke beta's uitbrengen elke 3 uur gedurende de dag. Dat is tussen 7 en 22 uur." | `.claude/skills/beta-release-2x/SKILL.md` (CHECKPOINT 1), AGENT-CORE block hard-stop exception |
| 2026-11-27 | Beta release notes: always say which builds are in the beta and what was added, completely, in plain language without programming terms; short plain head first, per-range test provenance in an appendix (TASK-2969 R4) | `.claude/skills/beta-release-2x/SKILL.md` (Phase 1) |
| 2026-11-27 | The OpenTherm Gateway OTGW32 is one of the two supported hardware configurations for firmware 2.0 and may be named in docs and on GitHub; silent launch: no Discord announcement of the hardware until Robert says so (replaces the 2026-11-22 "prototype board" wording) | AGENT-CORE block (Discord line), `.claude/skills/beta-release-2x/SKILL.md`, TASK-3115 |

## Replaced directives

| Date | Old directive | Replaced by | Date replaced |
|---|---|---|---|
| 2026-11-25 | Public betas at any full hour from 07:00 to 23:00, at most one per hour (hourly slots) | At most one public beta per 3 hours, 07:00 to 22:00 | 2026-11-27 |
| 2026-10-22 | SAT ground truth ranking with the pinned thermo-nova code (7beaaf5) second, above our firmware | Our implementation plus the recorded decisions is leading; Alex Wijn's SAT is a reference (TASK-3013) | 2026-11-21 |
| 2026-11-04 | 2.0.0 stable waits until SAT low-load is field-validated (epic TASK-1412 closed, TASK-1412.16-26) | 2.0.0 stable gate = TASK-1740; the epic and its full gate go to 2.1.0, 2.0.0 keeps the reduced slice (Robert, DM 1571910380756668488, option A of the TASK-1412 proposal) | 2026-11-16 |
| before 2026-10-25 | Repeat an unanswered SAT question to Sergeant D every 12 hours | Pace questions to his presence: check hourly, re-ask as soon as he is back | 2026-10-25 |
| 2026-10-29 | Batch cadence: build and publish after every 4 finished tasks | Per-task cadence: each task bench-tested before its zip is published | 2026-10-30 |
| TASK-1014 | Flat single-flight rule for web UI API calls (one request in flight) | ADR-165: at most 2 in flight (TASK-1015 load study) | ADR-165 |
| ADR-165 (2026-07-06) | Web UI and REST gate at most 2 API calls in flight | ADR-300: the device admits by memory need up to 4; the web UI keeps at most what the device reports (`rest_max_inflight`), 2 when it reports none (Robert, 2026-11-26, question 16 answer A and acceptance of ADR-300) | 2026-11-26 |
| 2026-06-20 | `feature-2.0.0-esp32s3-async`, `feature-2.0.0-esp32s3-only`, `feature-dev-2.0.0-otgw32-esp32-sat-support` as work lines | Folded into `dev`; historical, not active | 2026-06-20 |
| before 2026-07-04 | Discord MCP in a Docker container plus the `gustycube` Node server | Native Java `saseq/discord-mcp` process, one instance per token | 2026-07-04 |
| before TASK-1160 | `flash_otgw --update --app` also wrote a littlefs image found in `build\` | `--update --app <bin>` writes the app only and keeps the settings | TASK-1160 |
| before 2026-06-02 | Platform abstraction in per-platform headers incl. an ESP8266 header | The project's own Platform library under `src/libraries/Platform/src/` (c880a0203) | 2026-06-02 |

## How to add an entry

A new maintainer directive goes into the instruction file where it applies (CLAUDE.md core, a rule or a
guide) and gets a row under "Directives in force". When it replaces an earlier one, remove the old text from the
running instructions, move its row to "Replaced directives" and name the date it was replaced.
