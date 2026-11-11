# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.


# OTGW-firmware: Claude Instructions

---

<!-- AGENT-CORE START (TASK-1625: identical in CLAUDE.md, AGENTS.md and .github/copilot-instructions.md; guarded by tests/test_agent_core_block.py) -->
## Core rules for every agent

This block is shared word for word by CLAUDE.md, AGENTS.md (Codex) and .github/copilot-instructions.md (Copilot). CLAUDE.md with its `.claude/rules/` files holds the full rule set; where another instruction file disagrees with it, CLAUDE.md wins. Change this block in all three files at once.

- **Lines.** `dev` is the 2.x line: ESP32-S3 only (OTGW32, and the OTGW Classic with an ESP32-S3 module), async and FreeRTOS. `otgw-1.x.x` is the 1.x ESP8266 maintenance line. `main` is the latest public release and is only pushed in the release flow, after Robert's go.
- **Roles.** Robert (maintainer) decides scope, priority, waivers and releases; put an open decision to him as one question with the options, their consequences and your recommendation, by Discord DM (via the coordinator), never as an interactive CLI question; without a DM channel, write it asynchronously in the session and keep working. A hard stop (release, PIC flash, push to main, force-push, merging someone else's PR) needs his explicit yes for that one action, as a reply to a DM that names the action and its target; a general "go on" is no confirmation. Sergeant D decides SAT behaviour (see the SAT ground truth block). Testers give input and evidence, never decisions: their requests become tasks labelled `needs-maintainer-ok`.
- **Tasks.** Every change starts from a Backlog.md task. Use the `backlog` CLI for every task read and write. Never use the backlog MCP server and never edit `backlog/tasks/*.md` directly.
- **Done needs evidence.** Tick an acceptance criterion only with evidence from the session that produced it: build output, a test run, a capture, a screenshot. For a bug fix, the same reproduction fails on the old code and passes on the fix.
- **Flashing and bench tests.** Only the bench-tester agent (`otgw-bench-tester`) flashes a unit, over USB or OTA, and runs bench tests. It manages every hardware unit on the laptop and chooses the tests. Other agents never flash: the developer builds, then asks the coordinator for a bench run with the githash, the absolute artifact path and the behaviour to check. The bench-tester records the evidence and ticks the bench ACs; the developer pushes after its OK.
- **Firmware rules.** No ArduinoJson (ADR-146). No HTTPS or WSS. Never write to `Serial` after init (it is the PIC link); debug output goes to telnet. PIC commands only through `addCommandToQueue()`. String literals stay in flash (`F()`, `PSTR()`, the `_P` functions). No `String` class in hot paths (ADR-180). No raw `#ifdef ESP32`/`ESP8266` outside the Platform library: use the `platformXxx()` shims and `HAS_*` flags. The web UI keeps at most 2 API calls in flight.
- **Versions and commits.** A commit that touches `src/OTGW-firmware/**` or `src/libraries/**` bumps `_VERSION_PRERELEASE` with `bin/bump-prerelease.sh`. Every `TASK-NNN` in a commit message needs its task file staged. The git hooks in `.githooks/` enforce both (`git config core.hooksPath .githooks`).
- **Finish every lane.** Push only after the gates and announced freezes allow it, then `git fetch --prune` and bring your own tree up to date with origin. Once `git cherry origin/dev <branch>` shows your work landed, delete your lane branch locally and on the remote and remove your worktree. Touch only your own branches and worktrees: another session's local commits, dirty tree or branch go to the coordinator. Leftover directories move to `RvdB/OTGW-worktree-leftovers/`, never a blind delete. Checklist: `docs/guides/worktrees.md` (Finish a lane).
- **Gates.** `python evaluate.py --quick` shows no new FAIL. Build only through `build.bat` / `./build.sh`, never `python build.py` directly. Run the tests with `python scripts/run_tests.py`.
- **Discord.** Answer in English, answer first, with the real `<@ID>` mention. Never name the OTGW32 board in public. Every SAT correction or decision by Sergeant D goes into `docs/sat/sergeant-d-corrections.md` before it is implemented.
<!-- AGENT-CORE END -->

---

## Tasks (MANDATORY)

Every piece of work has a backlog task before any code is written. Use the `backlog` CLI for every read and write; never the `mcp__backlog__*` server (it indexes one worktree and serves stale state), never a direct edit of `backlog/tasks/*.md`. Full CLI reference: `docs/guides/backlog-cli.md`.

```bash
backlog search "<topic>" --plain                    # 1. find an existing task
backlog task create "Title" -d "..." --ac "..."     # 2. create one if none
backlog task edit <id> -s "In Progress" -a @claude  # 3. FIRST action on pickup, before any reading or code
backlog task edit <id> --plan "..."                 # 4. plan; share with Robert and wait (not inside /implement-next-task)
backlog task edit <id> --check-ac 1                 # tick ACs as their evidence comes in
backlog task edit <id> --append-notes "..."         # log progress
backlog task edit <id> --final-summary "..."        # evidence per AC
backlog task edit <id> -s Done
```

Enforced: `.claude/hooks/backlog-mcp-guard.py` blocks Edit/Write on task files; `.githooks/commit-msg` rejects a `TASK-NNN` without its task file staged. **Windows:** `backlog.cmd` cuts multi-line arguments at the first newline and silently drops later flags (TASK-1635); call `node <npm-prefix>/node_modules/backlog.md/cli.js` directly or use one `--append-notes` per paragraph. For SAT work under TASK-1412 read `docs/sat/sat-low-load-agent-handoff-2026-10-28.md` before selecting a task.

**Done only with evidence.** Set Done as soon as every AC and DoD item holds with evidence from this session (fresh build/evaluator output, a capture, a curl response, a screenshot, a bench log) and the Final Summary names it per AC. Run `docs/guides/pr-checklist.md` first and invoke `superpowers:verification-before-completion` before claiming anything done. Leave a task In Progress only when an AC is genuinely not self-verifiable (name it), a DoD item is unmet, or a sibling task gates it; attempt a verification before calling it impossible.

**Auto-advance.** After a task, pick up the highest-priority task whose non-field ACs are unblocked, without waiting; report and idle when everything is blocked. Do not advance when Robert asked to stop, when the next task needs a plan checkpoint, or for a cross-worktree master-plan task. Inside `/implement-next-task` (`.claude/skills/implement-next-task/SKILL.md`) the plan wait is replaced by an advisor self-review plus an #alpha-testing notice; PIC flash, release, push to `main` and force-push still stop the cycle.

## Design Principles

- **KISS**: Simplest solution that works. Share design choices so user decides on complexity.
- **YAGNI**: No features for hypothetical future requirements.
- **Minimal change surface**: Small, focused changes. Each change needs a concrete justification.
- **Comments about the present only**: Avoid defensive comments about hypothetical future scenarios ("if mode X is ever added, revisit this"). They confuse future readers by implying a plan that doesn't exist. Write only about what is true now. If the future concern is real, it belongs in a backlog task, not a code comment.
- **Fix the doc, not the identifier**: When an existing identifier's name is semantically correct but its documentation is wrong (stale docstring, mismatched comment, outdated reference), prefer fixing the documentation over renaming. A rename touching N call sites is rarely a net win when the name itself isn't the bug.
- **Surface assumptions, don't hide confusion**: If a request is ambiguous or a simpler path exists, say so before coding. Do not silently pick between interpretations; name them and ask.
- **Open questions go to Robert by Discord DM, never as an interactive CLI question, with a weighed recommendation** (maintainer directive 2026-10-29, route changed 2026-11-12, TASK-1829): when a decision is open (scope, a trade-off, an AC that cannot be evidenced, a waiver, a priority, anything only the maintainer can weigh), put it to Robert by Discord DM (his account `384411356616720384`; the coordinator owns the DM channel and relays the answer): one question per message, the concrete options with their consequences, and your recommendation, critically weighed (what you considered, the trade-off, why it wins, the main risk). Never use AskUserQuestion or another blocking question: Robert, 2026-11-12: "Vragen die je wilt stellen aan mij mogen niet meer interactief via de CLI, maar mag je via DM aan mij voorleggen." Keep working on what is not blocked while you wait. Only when the DM channel is not available, write the question asynchronously as plain text in the session. Hard stops may be confirmed by DM too (Robert, DM 1570221009511125012, 2026-11-12: "B"), but only by an explicit yes for that one action, as a reply to a DM that names the action and its target (version, branch, unit). Do not decide it silently, do not leave it only in task notes, and do not bundle several open decisions in one message. SAT design questions still go to Sergeant D under `.claude/rules/sat-ground-truth.md`.
- **Verifiable goals over vague intent**: Translate the task into a check before writing code ("add validation" → "tests for invalid inputs pass"; "fix bug" → "a test reproduces it, then passes"). For backlog work the AC checkboxes are the verification: if an AC is too vague to verify, sharpen it before implementing.
- **Reuse open source before building your own** (maintainer directive, 2026-10-14): before writing a library, tool or non-trivial helper, research existing open-source projects that already solve the problem (maintained, licence-compatible, fits our constraints: RAM/flash budget, no ArduinoJson per ADR-146, no HTTPS, ESP32-S3 + our pinned platform). Compare at least two candidates on primary sources (repo, release activity, licence, measured size/behaviour), and record what was checked and why it was chosen or rejected in the task notes. Build it yourself only when that research shows no candidate fits, and say why. Example: TASK-1224 uses the rjsmin/rcssmin minifiers instead of a hand-written one.

---

## Project and firmware rules

Firmware for the NodoShop OpenTherm Gateway: web UI, MQTT, REST API, TCP serial bridge, Home Assistant integration. Arduino C/C++, single translation unit. `dev` (2.x) is ESP32-S3 only, one esp32-combo image for the OTGW32 (OT-Direct) and the OTGW Classic with an S3 module (PIC); `otgw-1.x.x` is the ESP8266 1.x line; `main` is the latest public release. Port fixes between the lines deliberately, not reflexively.

The firmware rules are in the AGENT-CORE block above. The full set (PROGMEM, platform abstraction and its baseline ratchet, architecture rules, settings/state, REST, constraints, debugging user reports against the OpenTherm spec first) is `.claude/rules/firmware-coding.md`, and the read-before-you-touch table is `.claude/rules/navigation.md`. Both load automatically when you read or edit a file under `src/`; read them yourself before you plan firmware work without opening a source file. If you cannot name the C4 component that owns the code you are about to change, read `docs/c4/c4-context.md` first.

## ADR Guidelines

ADRs in `docs/adr/`. Read before changes to: architecture, NFRs, API contracts, new dependencies.

**Binding ADRs** (pattern-level, enforced by `evaluate.py` or tests — see ADR-080). The ADR frontmatter `binding: true` is the source of truth (TASK-1183): `scripts/adr_governance.py` reads it, and `tests/test_adr_governance.py` fails when this list differs from it or when this block differs between CLAUDE.md and AGENTS.md:
- **ADR-088**: MQTT status-burst windowing + post-burst cooldown (gated by `check_status_publishers_wrap_burst`, `check_status_burst_cooldown_bound`, `check_drip_consults_deferred` in evaluate.py)
- **ADR-091**: Design-system class drift gate (gated by `check_design_system_drift` in evaluate.py; WARN for one release, TASK-480 promotes to FAIL)
- **ADR-097**: MQTT publish gating by source and per-MsgID slave-echo classification (gated by `check_ps_summary_master_topic_gate` in evaluate.py)
- **ADR-101**: Flat per-value MQTT topics over aggregated JSON payloads (gated by `bin/adr-judge`)
- **ADR-146**: No ArduinoJson: the hand-rolled streaming JSON writer (JsonEmit) on the ESP32-S3 REST path (gated by `check_no_arduinojson` in evaluate.py)
- **ADR-177**: Keep the PIC byte stream verbatim from the serial task to every consumer (gated by the adr-judge declarative rules on the raw-path files)
- **ADR-201**: Combo board detection persists only a positive PIC verdict and re-probes every boot (gated by `tests/test_board_detect_persist_gate.py`, which compiles the real detection blocks and fails on a mutant that persists a no-PIC verdict)
- **ADR-217**: While SAT is enabled the Classic port 25238 bridge filters inbound lines and drops CV control commands and binary data; byte-transparent with SAT off (gated by `tests/test_bridge25238_sat_filter.py`, which compiles the real `handlePICSerial()` and fails on the code from before the ADR)

**Structural / architectural ADRs** (reviewed at PR, no automated gate — see ADR-080):
- **ADR-044**: Single-point-of-instantiation for globals
- **ADR-051**: Settings/State architecture (dual encapsulating structs, Hungarian prefix, two-level sections)
- **ADR-056**: Protected admin endpoint security and secret-handling contract (HTTP Basic Auth + CSRF same-origin enforcement on admin routes; supersedes ADR-054)
- **ADR-077**: Streaming MQTT HA discovery architecture
- **ADR-078**: MQTT sub-command dispatch tables (replaces chained `strcasecmp_P` blocks)
- **ADR-079**: Per-component type headers (`<Component>types.h` pattern, amendment to ADR-051)
- **ADR-080**: Binding ADR rules must have a CI gate (meta-rule)
- **ADR-081**: Types merge into `<Component>stuff.h` when both exist (amendment to ADR-079)
- **ADR-090**: Re-entrancy guard pattern for shared scratch buffers (guideline-level per ADR-080: 2 instances in MQTTstuff.ino, RAII `MQTTAutoConfigSessionLock` preferred for new code)
- **ADR-167**: Heap tier machine and per-consumer gating retired on this branch (supersedes ADR-089 and ADR-121). `getHeapHealth()` keeps ADR-030's 3072/5120/8192 ladder for emergency recovery and tier-entry telemetry only; `canSendWebSocket()` / `canPublishMQTT()` block on CRITICAL and nothing else. No CI gate: the four `evaluate.py` gates that enforced ADR-089/ADR-121 were removed with the code (TASK-1036)
- **ADR-180**: No `String` class in hot-path source files (SAT*, MQTTstuff, restAPI, OTGW-Core, OTDirect); guideline-level per ADR-080, `evaluate.py` reports WARN with call sites. Re-homes the rule from the superseded ADR-004

Accepted ADRs are binding. To reverse: new ADR that supersedes old one.

Format: Status / Context / Decision / Consequences / Related

Create an ADR when: architecture changes, new/replaced dependency, API contract change, build tooling change.
Do NOT create for: refactors, bug fixes, minor features within existing patterns.

Per **ADR-080**, a new pattern-level ADR MUST either reference its CI gate (in `evaluate.py` or `tests/`) or be explicitly labeled guideline-level in its Status line. No more "binding on paper, unchecked in practice".

## Build, flash and bench (hard rules)

- Build only with `build.bat` / `./build.sh`; never `python build.py`, `python flash_esp.py` or a direct `pio run` (wrong host Python gives a lying exit 0, and pio bypasses the lock).
- One build at a time on this machine across all sessions and worktrees (machine-wide lock in `build.py`, TASK-1488); at most 4 compiler jobs; on `dev` build `esp32-combo` only. Rebase onto origin/dev before building. Announce the build to the coordinating 2.0.0 session before it starts and when it ends, and respect a build or push freeze it announces.
- Per firmware task: the developer runs host tests red on the old code and green on the fix, `python evaluate.py --quick`, its own prerelease bump, a commit and an esp32-combo build, then requests a bench run from `otgw-bench-tester` through the coordinator. The bench-tester flashes that build, checks the device reports the githash, runs the tests it chooses and ticks the bench ACs; only after its OK does the developer push and publish (`.claude/rules/discord.md`).
- Only the bench-tester flashes (maintainer directive, 2026-11-11). It flashes over WiFi OTA first; USB only when OTA cannot work, and then `flash_otgw.bat --board esp32 --migrate --host <ip>`. A filesystem write resets the settings: save `/api/v2/settings` first. Never flash PIC firmware over WiFi using OTmonitor (bricks the PIC).
- Details (commands, targets, asset names, safeboot partitions, test automation and soak recipes): `docs/guides/agent-build-and-bench.md`.

## Bug fixing: reproduce, then prove (maintainer rule, Robert)

1. **Every new bug is made reproducible.** No capture logs from the reporter →
   always ask for them (`capture-mqtt-debug.bat` / `capture-otgw.sh` transcript,
   plus whatever the symptom needs: broker log, HA log, device/info counters).
   A description alone is not enough to fix against.
2. **Read the code critically, and be very critical of your own proposed fix.**
   More important still: collect evidence (logging, captures, browser
   screenshots). Do not believe you fixed something; prove it, with real tests.
3. **Best evidence = the real situation**, else a faithful simulation or a test
   harness that reproduces the problem. Run it against the OLD code (shows the
   failure) and against the FIX (shows it gone). Old-vs-fix on the same
   reproduction is the proof; a green run on the fix alone is not.

A host test that re-implements the code under test instead of calling it proves
nothing about that code. Say so when a test cannot reach the real function, and
close the gap on the bench.

---

## Git push policy

The default Claude Code instruction is "do not push without explicit user permission". For this project, the maintainer (Robert) has granted standing permission to push to **`origin/dev`** (the 2.0.0 async default line) and **`origin/otgw-1.x.x`** (the 1.x maintenance line) when it is logical to do so. Logical means: a clean working state, recent commits that are self-contained, and no pending review checkpoints.

Concrete rules that override the default "ask first":

- **`origin/dev`** push: allowed once a task is committed locally AND the build verifies (`python build.py` returns exit 0 for the relevant ESP32 target — `dev` is 2.0.0 ESP32-S3, no esp8266 target here; a plain build makes esp32-combo only, which is the only target built on dev, also for `HAS_*`/`boards.h`/platform/`platformio.ini` changes: maintainer 2026-10-29) AND the evaluator is green (`python evaluate.py --quick` shows no new failures). Mention the push in the user-facing summary. **Docs-only commits** (`*.md`, `docs/**`, `backlog/**`, `.claude/**`) may skip both gates — they cannot affect firmware compilation.
- **`origin/otgw-1.x.x`** push: allowed under the same conditions as `origin/dev` (task committed locally, build green for the esp8266 target, evaluator green; docs-only commits skip both gates). This is the 1.5.x/1.6.x maintenance/LTS line (the former `dev`).
- **`origin/main`** push: **`main` is ALWAYS the latest public release.** Never auto-pushed; every push requires explicit per-instance confirmation and is part of the release flow only.
- **Force-push** to any branch: still requires explicit per-instance confirmation. Force-push to main is forbidden regardless.
- **`origin/release/2.x`** push: the stable 2.x line (maintainer decision 2026-11-01, TASK-1545). Pushed only inside `/release-2x`, after Robert's go for that release; never force-pushed. It never merges back into `dev` as a whole (its production commit comments out `_VERSION_PRERELEASE`); release docs go to `dev` by cherry-pick. `main` stays the 1.x stable line until 1.x is end-of-life.
- **Sub-branches / other remote branches** (`feature-*`, `fix-*`, ad-hoc lanes): require explicit per-instance confirmation. Create a parallel lane only for genuine parallel work and delete it immediately after merging back (one-line policy, see `docs/guides/worktrees.md`).

When in doubt about whether a push is "logical", err toward asking. The cost of one extra prompt is small; the cost of an unwanted force-push is large.

## Release flows

| Line | Beta / prerelease | Stable release |
|---|---|---|
| 2.x (`dev`, ESP32-S3) | `/beta-release-2x` (`.claude/skills/beta-release-2x/SKILL.md`): tag on `dev`, GitHub prerelease with `--latest=false` | `/release-2x` (`.claude/skills/release-2x/SKILL.md`): tag on `release/2.x` |
| 1.x (`otgw-1.x.x`, ESP8266) | `/beta-prerelease` | `/release` (`otgw-1.x.x` -> `main`) |

Docs: `/update-docs-2x` (`.claude/skills/update-docs-2x/SKILL.md`, scope from `scripts/docs-scope-2x.sh`) on the 2.x line, standalone or in release mode; `/update-docs` on the 1.x line.

`dev` has no release CI: 2.x builds are local (esp32-combo) and assets are uploaded with `gh`, draft first (immutable releases).

---

## Versions and worktrees

A commit that touches `src/OTGW-firmware/**` (except `version.h`) or `src/libraries/**` carries its own prerelease bump via `bin/bump-prerelease.sh`, which stages every banner it touched; never pass `--git`. Build-stamp churn in `version.h`/`version.hash` is not a bump: restore it. Full policy: `docs/guides/versioning-policy.md`.

Single-lane work commits straight on `dev`. A sub-branch or extra worktree only for a genuine parallel lane, deleted right after the merge. One worktree per live session; never `git add -A` in a shared tree; never cross lines inside one tree. Cross-line work (dev and otgw-1.x.x) gets one master plan approved by Robert, then one task and one agent per line. Details and recovery recipes: `docs/guides/worktrees.md`.

## ADR kit and graphify

- Before a feature or architectural change run `/adr-kit:context <topic>` and honour the Accepted ADRs it returns. Author ADRs with `/adr-kit:adr` (or the `adr-generator` subagent). While coding follow the plugin's `instructions/adr.coding.md`; in a review apply the six named ADR checks of `instructions/adr.review.md`. An ADR goes Proposed to Accepted only through the four gates (Completeness, Evidence, Clarity, Consistency) and is then immutable: supersede, never edit.
- For questions about firmware code structure run `graphify query|path|explain` first; the graph covers `src/` only. Read `graphify-out/GRAPH_REPORT.md` only for a broad architecture review. The graph complements the C4 reading rules, never replaces them. After changing `src/` run `graphify update src`, never `graphify update .`.

## Where the rest lives

| Topic | File | Loaded |
|---|---|---|
| SAT ground truth and Sergeant D rules | `.claude/rules/sat-ground-truth.md` | every session |
| Discord MCP, alpha-tag publishing | `.claude/rules/discord.md` | every session |
| Firmware coding rules | `.claude/rules/firmware-coding.md` | on `src/`, `test/`, `safeboot/` files |
| What to read before touching code | `.claude/rules/navigation.md` | on `src/`, `docs/c4/`, `other-projects/` files |
| Build, flash, bench, soak | `docs/guides/agent-build-and-bench.md` | read before building |
| Versioning policy | `docs/guides/versioning-policy.md` | read before a firmware commit |
| Worktrees and cross-line work | `docs/guides/worktrees.md` | read before a parallel lane |
| Dated maintainer directives and what replaced what | `docs/process/directive-changelog.md` | on demand |
| Discord MCP operations | `docs/guides/discord-mcp-server.md` | on demand |
