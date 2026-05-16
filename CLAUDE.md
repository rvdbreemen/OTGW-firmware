# OpenWolf

@.wolf/OPENWOLF.md

# OTGW-firmware: Claude Instructions

## Task Management (MANDATORY)

**Every piece of work must have a backlog task before any code is written. No exceptions.**

<!-- BACKLOG.MD GUIDELINES START -->
<!-- DO NOT regenerate this block via `backlog agents --update-instructions`. -->
<!-- The full Backlog CLI reference is intentionally extracted to -->
<!-- .claude/backlog-cli-reference.md to keep this file focused on project-specific context. -->

# Backlog.md task management

All task operations go through the **`backlog` CLI** — never edit task files directly. The CLI owns file naming, frontmatter, AC indexing, and Git tracking; bypassing it desynchronises the project.

Full CLI reference: @.claude/backlog-cli-reference.md

## Autonomous task completion (project policy)

When you've satisfied all 8 Definition-of-Done items from the reference (every AC checked, every DoD item checked, Final Summary added, build passes, evaluator green, no regressions) — set the task status to **Done** immediately. Do not leave the task at "In Progress" waiting for the user to flip it.

Rationale: if the AC list is well-designed, "all ACs checked" already means "the task is objectively done". Treating user review as a gating step doubles turnaround. The user audits after the fact via Final Summary and git log.

**Exceptions** (leave at "In Progress" only if):
- An AC genuinely cannot be self-verified (hardware-specific tester feedback, explicit user sign-off as policy, third-party integration approval) — document the blocking AC in the Final Summary.
- A DoD item is unmet (build failed, regression detected, missing test).
- Coordinated set where status transitions wait on a sibling.

If unsure whether an AC is self-verifiable, prefer to **attempt** verification rather than preemptively defer. Trust the AC list; if an AC was truly unverifiable, it shouldn't have been an AC.
<!-- BACKLOG.MD GUIDELINES END -->

---

## Design Principles

- **KISS**: Simplest solution that works. Share design choices so the user decides on complexity.
- **YAGNI**: No features for hypothetical future requirements.
- **Minimal change surface**: Small, focused changes. Each change needs a concrete justification.
- **Comments about the present only**: Don't write defensive comments about hypothetical future scenarios ("if mode X is ever added, revisit this"). They imply a plan that doesn't exist. Real concerns belong in a backlog task, not a code comment.
- **Fix the doc, not the identifier**: When a name is correct but a comment/docstring is stale, fix the comment. A rename touching N call sites is rarely a net win when the name itself isn't the bug.
- **Surface assumptions, don't hide confusion**: When a request is ambiguous or a simpler path exists, say so before coding. Don't pick silently between interpretations — name them and ask.
- **Verifiable goals over vague intent**: Translate tasks into a check before writing code ("add validation" → "tests for invalid inputs pass"; "fix bug" → "test reproduces it, then passes"). For backlog work, the AC checkboxes are the verification — if an AC is too vague to verify, sharpen it before implementing.
- For deeper behavioural guardrails (anti-rationalisation, surgical-edit discipline), invoke `/andrej-karpathy-skills:karpathy-guidelines`.

---

# OTGW-firmware Project Guidelines

## Project Overview

ESP8266 firmware for the NodoShop OpenTherm Gateway. Provides Web UI, MQTT, REST API, and TCP serial socket with Home Assistant integration focus.

- **Platform**: ESP8266 (NodeMCU / Wemos D1 mini), ~40KB usable RAM
- **Language**: Arduino C/C++ (.ino files), single translation unit
- **Serial**: Reserved exclusively for PIC communication — never write to Serial after init
- **Debug output**: Use `DebugTln()`, `DebugTf()`, etc. (telnet port 23), never Serial
- **This branch (`dev`)** is the 1.5.x maintenance line. The 2.0.0 ESP32/SAT feature line lives in the parallel `feature-dev-2.0.0-otgw32-esp32-sat-support` worktree. Default to the branch you are on; port fixes deliberately, not reflexively.

## Layout

- `src/OTGW-firmware/` — main firmware sketches (`*.ino`), `version.h`, `OTGWSettings.h`
- `src/OTGW-firmware/data/` — LittleFS web UI: `index.html`, `index.js`, `index.css`, `graph.js`, `FSexplorer.html/css`, fonts (shipped to device as a filesystem image)
- `src/libraries/` — vendored libs: `OTGWSerial`, `SimpleTelnet`, `OpenTherm`, `WebSockets`, …
- `docs/adr/` — Architecture Decision Records
- `docs/guides/` — operational guides (BUILD, FLASH, MQTT_LWT, WIFI_RECOVERY, …)
- `backlog/` — Backlog.md tasks, decisions, docs
- `.wolf/` — OpenWolf context cache (`anatomy.md`, `cerebrum.md`, `buglog.json`)
- `build.py`, `evaluate.py` — top-level Python build/QA scripts

## Runtime structure

Single translation unit — Arduino concatenates every `.ino` in `src/OTGW-firmware/` into one compilation. Entry points live in **`OTGW-firmware.ino`**:

- `setup()` — boot sequence: filesystem, settings, WiFi, MQTT, OTGW PIC reset, web/REST handlers.
- `loop()` — calls `doBackgroundTasks()` and yields.
- `doBackgroundTasks()` — the actual work loop: timers, queue draining, watchdog, MQTT publish. **Re-entrant** via `feedWatchDog()` → `yield()` (see "Static buffers, cooperative scheduling" below).

Sibling `.ino` files in the same directory are organised by feature (`MQTTstuff.ino`, `restAPI.ino`, `OTGW-Core.ino`, `networkStuff.ino`, `settingStuff.ino`, …). Each contributes free functions to the single translation unit; there are no class-based modules.

## Architecture Decision Records (ADRs)

ADRs live in `docs/adr/`. **Read relevant ADRs before making changes that affect architecture.**

### When to create an ADR
Create one when a change affects: architecture, NFRs (security/performance/availability), API contracts, new/replaced dependencies, or build/CI tooling.

Do NOT create ADRs for: pure refactors, bug fixes, minor features within existing patterns.

### ADR lifecycle

- **Proposed** → Draft; **Accepted** → Stands; **Deprecated** → No longer recommended; **Superseded** → Replaced

**CRITICAL: NEVER edit an Accepted or Deprecated ADR.** The content of these ADRs is immutable — do not change their text, parameters, numbers, or any other content. The ONLY permitted change to an Accepted/Deprecated ADR is updating its Status field to "Superseded by ADR-XXX".

To change a decision: create a NEW ADR with the next available number that supersedes the old one, then mark the old ADR's status as "Superseded by ADR-XXX". Only **Proposed** ADRs may be freely edited.

### ADR creation workflow (human checkpoints required)

1. **Create** the ADR file with `Status: Proposed`. Include Context, Decision, Consequences, and Related sections.
2. **Stop and ask the user to review.** Do not proceed until the user explicitly approves or requests changes. Present the key trade-offs and ask for confirmation.
3. **Iterate** on feedback — edit the Proposed ADR as needed.
4. **Only set `Status: Accepted` after the user explicitly approves.** Never self-approve an ADR. The user must say the ADR is accepted before you change the status.

This applies equally to new ADRs and to ADRs that supersede existing ones.

### ADR format
```
# ADR-NNN-Short-Title
## Status: Proposed | Accepted | Deprecated | Superseded
## Context — problem, constraints, alternatives considered
## Decision — chosen approach and rationale
## Consequences — benefits, trade-offs, risks
## Related — relevant code, issues, PRs, prior ADRs
```

<!-- ADR-KIT STUB START -->
<!-- DO NOT regenerate manually. Updated by `/adr-kit:init`, `/adr-kit:upgrade`, `/adr-kit:setup`. -->
## ADR Kit

This project uses [adr-kit](https://github.com/rvdbreemen/adr-kit). All architectural decisions live as ADRs in `docs/adr/`. Full guide: @.claude/adr-kit-guide.md

Authoring: `/adr-kit:adr` (or the `adr-generator` subagent).
Pre-commit verification: `bin/adr-judge` runs declarative `Enforcement` rules at commit time. ADRs with `llm_judge: true` are reviewed in-session via `/adr-kit:judge`.
<!-- ADR-KIT STUB END -->

## Naming Conventions

- **Variables**: camelCase (`settingHostname`, `lastReset`)
- **Constants/defines**: UPPER_CASE (`ON`, `OFF`, `CMSG_SIZE`)
- **Functions**: camelCase (`startWiFi`, `readSettings`)
- **Global settings**: prefix with `setting` (`settingMqttBroker`)

## Critical Coding Rules

### PROGMEM — MANDATORY (ESP8266 has ~40KB RAM)

All string literals MUST stay in flash, not RAM:
```cpp
DebugTln(F("Message"));                              // F() for functions supporting it
DebugTf(PSTR("Value: %d\r\n"), value);               // PSTR() for printf-style
snprintf_P(buf, size, PSTR("Format: %s"), str);      // snprintf_P for all formatting
const char myStr[] PROGMEM = "Long string";          // PROGMEM for string constants
```
For string comparisons: use `strcmp_P()`, `strcasecmp_P()` with `PSTR()`.

**Post-mortem rule**: If a bug involves `_P` helpers, `PGM_P`, or `__FlashStringHelper`, assume a storage-domain mismatch until proven otherwise. The RAM/flash domain must match the helper — never pass a PROGMEM pointer where RAM is expected or vice versa.

### PROGMEM pointer safety on Arduino Core 3.1.2+
Standard C functions (`strstr`, `strncmp`, `strlen`) may use word-aligned reads internally.
On ESP8266, unaligned 32-bit reads from flash (0x402xxxxx) cause Exception (3).
Use `pgm_strncmp_PP()` and `pgm_read_char()` (defined in `MQTTstuff.h`) for safe byte access.
Never pass PROGMEM pointers to `printf %s`, `MQTTclient.write()`, or `writeMqttChunk()`.
Use `writeMqttProgmemChunk()` for PROGMEM data to MQTT.

### No String class in hot paths (ADR-004)
- Use `char[]` buffers with `strlcpy`, `strncat`, `snprintf_P`
- `String` is only acceptable in setup/init code or truly one-off contexts
- Heap fragmentation on ESP8266 is a real stability concern

### No ArduinoJson — build JSON manually
JSON output uses `snprintf_P` and helpers like `sendJsonMapEntry`. Parsing uses `parseJsonKVLine()`. ArduinoJson's allocator fragments the heap and the streaming-discovery code (ADR-042) explicitly avoids it. Don't introduce it for new JSON paths.

### Binary data: use memcmp_P, never strncmp_P/strstr_P (CRITICAL)
`strncmp_P`/`strstr_P` on binary data causes Exception (2) crashes. Use:
```cpp
if (memcmp_P(datamem + ptr, banner, bannerLen) == 0) { ... }  // CORRECT
```

### File serving — stream, never load into RAM
- Files >2KB: MUST use `httpServer.streamFile(f, mimeType)` or chunked transfer
- Files >10KB: CRITICAL — always stream, loading causes crash
- `index.html` is ~11KB — never call `f.readString()` on it

### Network — HTTP/WS only (no HTTPS/WSS)
This firmware uses plain HTTP and WS protocols only. **Never add HTTPS or WSS support.**
- Target environment: local network only, not internet-exposed
- Security model: trusted LAN; use VPN for remote access
- Reverse proxy: REST API works behind HTTPS proxy, but WebSocket (live OT log) assumes plain HTTP

### Typed internal control flow
Use `enum class`, numeric IDs, or flags for internal behavior selection — not string tokens. Internal discriminator strings are fragile on ESP8266 and can hide RAM-vs-flash pointer bugs.

### Browser compatibility (frontend code)
All frontend JavaScript must work in Chrome, Firefox, and Safari (latest + 2 versions back). Always check element existence before DOM access, wrap `JSON.parse()` in try-catch, check `response.ok` on fetch, and add `.catch()` on all async operations.

**Log container contract:** `.ot-log-content` has `white-space: pre` (in `index.css`); `\n` is the line separator. Prefer `textContent` over `innerHTML` for plain text — skips the HTML parser and per-line escape, and avoids accidental injection from log content.

### Static buffers, cooperative scheduling
- Re-entrancy: `doBackgroundTasks()` can be re-entered via `feedWatchDog()` → `yield()`
- Buffers shared across a yield window must be local/static, not global scratch buffers
- `mqttAutoCfgScratch` and `ot_log_buffer` have documented ownership rules — respect them

### Timer management
Use `DECLARE_TIMER_SEC()` / `DECLARE_TIMER_MS()` macros and check with `DUE()`. See `safeTimers.h`.

### Command queue
Never send commands directly to the PIC serial port. Always use `addOTWGcmdtoqueue()`.
Never flash PIC firmware over WiFi using OTmonitor — it can brick the PIC.

## Testing model

No automated unit/integration tests exist. Validation pipeline:
1. `python build.py --firmware` exits 0 (firmware compiles clean)
2. `python evaluate.py --quick` shows no new failures (PROGMEM/safety lint)
3. Beta build deployed via OTA; field validation in Discord `#beta-testing`

Don't look for jest/pytest — there isn't a runner. Hardware-in-the-loop is the only behavioural test.

## Build Commands

Preferred wrapper (handles venv setup): `./build.sh` (macOS/Linux) or `build.bat` (Windows). Both invoke `build.py` underneath and build firmware + filesystem. Use a direct `python build.py` invocation only when the wrapper is unavailable.

```bash
./build.sh                   # Preferred — firmware + filesystem (handles venv)
python build.py              # Build firmware + filesystem (no venv handling)
python build.py --firmware   # Firmware only (also the push-policy gate)
python build.py --clean      # Clean build
python evaluate.py           # Code quality check (PROGMEM, unsafe patterns)
python evaluate.py --quick   # Fast check
```

## Settings & State Architecture (ADR-051)

- `OTGWSettings settings` — persistent, serialized to LittleFS as JSON
- `OTGWState state` — transient runtime state, never persisted
- Both use two-level named sub-sections with Hungarian prefixes (b=bool, s=char[], i=int, f=float)
- Access as: `settings.mqtt.sBroker`, `state.otgw.bOnline`, etc.

## REST API Versioning

- `/api/v0/` — legacy; `/api/v1/` — standard; `/api/v2/` — current preferred
- Dispatch table in `restAPI.ino` (`kV2Routes[]`) — add new endpoints by adding one entry
- Always return JSON errors via `sendApiError(httpCode, F("message"))`

## Project skills

Three OTGW-specific skills under `.claude/skills/`:

- **`adr`** — invoke when authoring or reviewing an Architecture Decision Record (`docs/adr/`).
- **`flash`** — invoke to build firmware + filesystem and flash to a USB-connected ESP. Auto-detects the serial port; no input required.
- **`release`** — invoke to prepare and execute a full release following the documented process.

Generic Anthropic-published skills (`pdf`, `docx`, `refactor`, `webapp-testing`, …) live under `.github/skills/` for Copilot. Consult them only when the task is actually about that domain (e.g., extracting from a PIC datasheet PDF).

## Superpowers skills

**At the start of every conversation, invoke `superpowers:using-superpowers` via the `Skill` tool before doing anything else.** This establishes the skill-discovery flow: if any installed skill might apply to the current task — even at low confidence — invoke it via the `Skill` tool before responding. Don't paraphrase a skill from memory; the on-disk version may have evolved.

Two superpowers skills are particularly useful in this codebase:

- **`superpowers:verification-before-completion`** — invoke before claiming any work done (build green, fix shipped, regression resolved, push complete). This project has many field-validation gates that tempt premature "done" claims; the skill enforces *fresh* evidence (re-run the verification command in the current message, read the actual output) instead of relying on prior runs or extrapolation. Especially relevant before flipping AC checkboxes, marking tasks Done, or pushing to `origin/dev` / `origin/feature-dev-2.0.0-otgw32-esp32-sat-support`.
- **`superpowers:brainstorming`** — invoke before entering plan mode for any non-trivial feature or refactor. Pairs well with the cross-worktree master-plan rule (Worktree layout § Cross-worktree work) when a change spans both branches.

Other superpowers skills (`debugging`, `frontend-design`, `mcp-builder`, …) — invoke whenever the task description matches the skill's stated domain. The bar is low: a 1% chance of fit means use it.

## Git push policy

The default Claude Code instruction is "do not push without explicit user permission". For this project, the maintainer (Robert) has granted standing permission to push to **`origin/dev`** and **`origin/feature-dev-2.0.0-otgw32-esp32-sat-support`** when it is logical to do so. Logical means: a clean working state, recent commits that are self-contained, and no pending review checkpoints.

Concrete rules that override the default "ask first":

- **`origin/dev`** push: allowed once a feature task is committed locally AND the build verifies (`python build.py --firmware` returns exit 0) AND the evaluator is green (`python evaluate.py --quick` shows no new failures). Mention the push in the user-facing summary. **Docs-only commits** (`*.md`, `docs/**`, `backlog/**`, `.claude/**`) may skip both gates — they cannot affect firmware compilation.
- **`origin/feature-dev-2.0.0-otgw32-esp32-sat-support`** push: allowed under the same conditions as `origin/dev` (feature task committed locally, build green for the relevant target, evaluator green; docs-only commits skip both gates). This is the active 2.0.0 development line; auto-push reduces the friction of cross-branch porting work that this branch carries from dev. Mention the push in the user-facing summary.
- **`origin/main`** push: still requires explicit per-instance confirmation. Main is release-line; never auto-pushed.
- **Force-push** to any branch: still requires explicit per-instance confirmation. Force-push to main is forbidden regardless.
- **Other remote branches** (`feature-*` other than the 2.0.0 line, `fix-*`, etc.): require explicit per-instance confirmation unless the user has granted standing permission for that specific branch in this same section.

When in doubt about whether a push is "logical", err toward asking. The cost of one extra prompt is small; the cost of an unwanted force-push is large.

## Versioning policy

Field testers on Discord identify issues by the version string ("on beta.23 I see..."), so each material firmware change must ship under its own prerelease tag (`_VERSION_PRERELEASE` in `src/OTGW-firmware/version.h`, currently `<word>.<N>` form, e.g. `beta.23`). Multiple commits batched under the same tag erase the testers' ability to A/B them.

- **What requires a bump** — any commit whose staged paths include `src/OTGW-firmware/**` (excluding `src/OTGW-firmware/version.h` itself) or `src/libraries/**`. The same commit must update `_VERSION_PRERELEASE` (and the cascaded `_SEMVER_*`/`_VERSION` lines and `data/version.hash` that `scripts/autoinc-semver.py` rewrites).
- **What does not** — docs-only / tooling-only commits: `*.md`, `docs/**`, `backlog/**`, `.claude/**`, `scripts/**`, `bin/**`, `.githooks/**`, top-level `.py`/`.sh`/`.bat`, and `data/version.hash` on its own. These cannot affect firmware behaviour and are exempt.
- **How to bump** — run `bin/bump-prerelease.sh` from the project root. It parses the current tag (must match `^[a-zA-Z]+\.[0-9]+$`), increments the trailing integer, and calls `scripts/autoinc-semver.py --prerelease <new>` to rewrite `version.h` + `data/version.hash` + the cascaded fields. The helper does NOT git-add — stage `src/OTGW-firmware/version.h` and `src/OTGW-firmware/data/version.hash` yourself alongside the firmware change.
- **Enforcement** — `.githooks/pre-commit` runs a bump-check after the adr-judge gate. If the staged set triggers and `git diff --cached -- src/OTGW-firmware/version.h` does not show a `+`/`-` pair on the `_VERSION_PRERELEASE` line, the commit is blocked with the path list and remediation hint.
- **Bypass** — `OTGW_BUMP_HOOK_DISABLE=1 git commit ...` skips the bump-check for one commit. Intended for cherry-picks, merges, or rebases where the bump rides on a separate commit. Do not use it to dodge bumping a real change.

## Worktree layout

This project is intentionally checked out into **two parallel git worktrees** so the 1.5.x release line and the 2.0.0 feature line can be worked on side-by-side without branch-switch churn:

| Worktree path | Branch | Purpose |
|---|---|---|
| `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware` | `dev` | 1.5.x release line — the default working tree |
| `D:\Users\Robert\Documents\GitHub\RvdB\OTGW-firmware-2.0.0` | `feature-dev-2.0.0-otgw32-esp32-sat-support` | 2.0.0 ESP32 + SAT feature line |

**The 2.0.0 worktree has its own `CLAUDE.md`** with ESP32/SAT-specific rules and a richer toolchain (C4 docs, hooks, adr-kit plugin, discord-mcp server). When working in that tree, those rules supersede this file's guidance. The two files are not synchronised — divergence is intentional, since the platforms and tooling differ.

**Rule: keep both worktrees present.** Work that targets one branch (e.g. SAT dashboard / ESP32 / 2.0.0 features) belongs in its own worktree; work targeting `dev` belongs in the dev worktree. Never use `git checkout <other-branch>` inside one worktree to do work that belongs in the other — it defeats the point of the split and risks losing in-flight changes on the original branch.

**If only one worktree exists, create the missing one before starting side-by-side work.** From inside the existing worktree:

```bash
# missing the 2.0.0 feature worktree:
git worktree add ../OTGW-firmware-2.0.0 feature-dev-2.0.0-otgw32-esp32-sat-support

# missing the dev worktree:
git worktree add ../OTGW-firmware dev
```

Verify with `git worktree list`.

**Backlog.md: prefer the `backlog` CLI for all task operations; fall back to `mcp__backlog__*` tools only when the CLI is unavailable.** The MCP server inherits the launching session's working directory and indexes only that single worktree. Tasks living in a sibling worktree are invisible to `mcp__backlog__task_search`, and `mcp__backlog__task_view` returns cached/stale content for cross-tree tasks (verified 2026-05-05: MCP kept returning the pre-edit "In Progress" snapshot of TASK-514 long after a CLI edit had marked it Done on disk in the 2.0.0 tree). Mixing CLI and MCP on the same task is fragile because MCP caches and does not reflect CLI-side writes without a server restart. Use `backlog task ...` CLI for every read, edit, create, complete, and archive.

**CLI cross-tree behaviour.** `backlog task <id> --plain` resolves from either worktree, but `backlog task edit` only writes to the worktree where the task file actually lives. If an edit returns `Task not found`, `find` for `task-<id>*` across both worktrees and run the edit from the worktree that holds the file. SAT / ESP32 / 2.0.0 tasks generally live in the feature worktree's `backlog/tasks/`, not in dev's.

**Fallback when both MCP and CLI are unavailable.** If `mcp__backlog__*` tools are not registered for the session AND `backlog` is not on `PATH` (fresh container, web session, or a worktree where the global install is missing), do NOT fall back to editing task files by hand. Instead, invoke the CLI through `npx`:

```bash
npx -y backlog.md task list --plain
npx -y backlog.md task <id> --plain
npx -y backlog.md task edit <id> -s "In Progress" -a @myself
```

`npx -y backlog.md` resolves to the same binary as the global `backlog` command and preserves all metadata/Git tracking guarantees. After the first invocation `npx` caches the package, so subsequent calls are fast. Only after `npx` itself fails (no network, no Node) should you escalate to the user — never bypass the CLI by hand-editing markdown.

### Cross-worktree work — ask first, then plan once, then parallelise

Whenever you take on a bug fix, feature change, or architectural decision, **explicitly ask yourself**: *does this also need to land on the other worktree?* For 1.5.x↔2.0.0 the answer is almost always **yes** when the change touches:

- Any file under `src/OTGW-firmware/` whose name is the same in both trees (most `.ino`, `.h`, `.cpp` and the LittleFS data assets) — even when the line numbers differ between branches, the architectural fix usually applies to both.
- ADRs that codify a cross-cutting decision (MQTT topic shape, heap policy, discovery semantics, settings schema). The dev ADR and the 2.0.0 ADR live in their own worktrees and have separate numbering, but the *decision* must be coherent across both.
- Anything driven by a HA-side, broker-side or PIC-side contract (HA discovery regex, MQTT retained behaviour, OpenTherm message ID semantics) — those are platform-independent and the firmware-side fix must apply on both branches.

The answer is usually **no** when the change is genuinely scoped to a feature that only exists on one branch (SAT dashboard, ESP32-S3 board pinning, OTGW32 hardware bring-up → 2.0.0 only; LTS-1.4.x patch backports → only that branch).

**If both: one master plan, two tasks, two agents.**

1. **Write ONE master plan first**, before creating any task or spawning any agent. The plan covers *both* worktrees in a single document and must contain, for each change:
   - The desired outcome in plain language (the "what" and "why").
   - The exact file(s) and line number(s) on **both** branches — they often differ. Read each branch's source to confirm; do not assume parity.
   - Per-platform considerations explicitly called out for the 2.0.0 side (ESP8266 vs ESP32-S3 deadbands/thresholds, board-specific pin maps, conditional compilation flags).
   - The full AC list each per-worktree task will inherit (build commands, evaluator commands, field-validation gates, ADR-acceptance gates).
   - Cross-tree dependencies and ordering — e.g. dev ADR-N must be Accepted before 2.0.0 ADR-M can be drafted; dev impl can be pushed before 2.0.0 impl is reviewed.
   - The expected commit message prefix and the push gate per branch (`origin/dev` auto-push allowed under the policy above; 2.0.0 feature branch needs explicit confirmation).

2. **Share the master plan with the user for approval** before any task creation. The user's "go" on the master plan replaces the per-task plan-review gate that would otherwise apply individually.

3. **Then create two separate backlog tasks**, one per worktree. Use the convention `feat-2.0.0: port TASK-N — <title>` for the 2.0.0 sibling so cross-references stay legible. Each task carries its own AC list derived from the master plan; do not over-share ACs across tasks (each agent must be able to verify its own task in isolation).

4. **Spawn two agents in parallel**, one per worktree. Each agent:
   - Has its own self-contained prompt referencing the master plan (or restating its scope in full).
   - Works exclusively in its own worktree and explicitly does not touch the other tree.
   - Reports back independently with build/evaluator/commit/push receipts.

5. **Verify both reports**, then report a consolidated summary to the user.

Do **not** sequence (dev first, then 2.0.0 only after dev is fully done) unless there's a hard ordering dependency (e.g. 2.0.0 ADR cites a regex finding from the dev ADR — in that case dev ADR must be Accepted before the 2.0.0 ADR can be drafted, but the 2.0.0 *code* impl can still parallelise once both ADRs are Accepted).

The benefit of writing the plan once is symmetry: agents see the same intent, the two ADRs cross-reference cleanly, the two commits land within minutes of each other, and the user reviews one design instead of two slightly-divergent designs.
