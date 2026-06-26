# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.


# OTGW-firmware: Claude Instructions

---

## Task Management (MANDATORY)

**Every piece of work must have a backlog task before any code is written. No exceptions.**

**Always use the `backlog` CLI for task operations on this project. Do NOT use the `mcp__backlog__*` MCP server.** The MCP server indexes only one worktree at a time and serves stale cached state across worktrees (verified 2026-05-05: MCP returned a pre-edit "In Progress" snapshot of TASK-514 long after the CLI marked it Done on disk in another tree). The "Backlog.md: always use the CLI" section near the bottom of this file is the canonical statement; this paragraph is a reminder so the rule is the first thing seen.

```bash
# Before writing any code:
backlog search "<topic>" --plain           # 1. Find existing task
backlog task create "Title" -d "..." --ac "..."  # 2. Create if none
backlog task edit <id> -s "In Progress" -a @claude  # 3. Start it
backlog task edit <id> --plan "..."        # 4. Write plan, share with user, WAIT for approval

# During implementation:
backlog task edit <id> --check-ac 1        # Mark ACs done as you go
backlog task edit <id> --append-notes "..."  # Log progress

# When done:
backlog task edit <id> --final-summary "..." # PR description
backlog task edit <id> -s Done
```

**CRITICAL: NEVER edit task files in `backlog/tasks/` directly. Always use the `backlog` CLI.**

Two hooks enforce this contract — they fail closed, you don't have to remember:

- `.claude/hooks/backlog-mcp-guard.py` — PreToolUse guard wired in `.claude/settings.json`. Blocks `Edit/Write/MultiEdit` on `backlog/tasks/*.md` (direct file edits are never allowed). Does NOT block `backlog` CLI invocations — CLI is the preferred interface.
- `.githooks/commit-msg` — git hook that fails the commit if its message references `TASK-NNN` without a matching `backlog/tasks/task-NNN*.md` file in the index. Catches the failure mode where a code commit lands but its task record stays untracked. Install once per clone with `git config core.hooksPath .githooks`. Bypass with `git commit --no-verify` for emergencies (document why in the message).

For the full CLI reference (all commands, AC management, DoD, multi-line input): read `docs/guides/backlog-cli.md`.

Before marking a task `Done`, run through `docs/guides/pr-checklist.md`. "Builds clean" is the lowest bar; the checklist captures the hardware / browser / MQTT smoke tests that build-clean doesn't see.

**Known bug:** `backlog task list` returns empty. Use `backlog task <id> --plain` or read `backlog/tasks/` directly.

## Task pickup (MANDATORY)

**When picking up any task from the backlog — whether newly created or already existing — the first action before any code, research, or file reading is:**

```bash
backlog task edit <id> -s "In Progress" -a @claude
```

This makes the task visible in the correct board column immediately. Skipping this step leaves the task in "To Do" while it is actually being worked on, which creates false visibility for the user and breaks board accuracy.

## Auto-advance to next task (project policy)

After completing a task (or reaching a blocking state with no self-verifiable ACs remaining), **immediately analyse the backlog and pick up the highest-priority actionable task** without waiting for the user to prompt. Apply the following selection order:

1. Highest-priority task with all non-field-validation ACs unblocked (can be started and verified without hardware).
2. If all open tasks are blocked on field validation or external input, report that state and idle.

Exceptions — do NOT auto-advance:
- User has explicitly asked you to stop or wait.
- The next task requires a plan approval checkpoint (per KISS principle: share choices so user decides on complexity).
- The next task is a cross-worktree master-plan task (requires the one-plan-then-two-agents protocol).

---

## MCP Servers

Two MCP servers are wired into Claude Code for this project:

- **`mcp__backlog__*`** — task management. Started locally by Claude Code itself (stdio).
- **`mcp__discord-mcp__*`** — Discord I/O for `#dev-sat-mqtt` and the support channels. Runs as a long-lived Docker container (`saseq/discord-mcp`) on `http://localhost:8085/mcp`, pre-loaded with `DISCORD_TOKEN` from the Windows user environment. Most-used tools: `read_messages`, `send_message`, `get_server_info`, `read_private_messages`, `send_private_message`. There is no separate login tool — the container handshakes on its own at startup. For attachment contents (logs, screenshots), see `.claude/commands/backlog_discord.md` (Phase 1b) and `.claude/commands/check_otgw_issues.md` (Phase 1d).

If Discord calls start failing with "Expected token to be set" or 401, the container session has drifted from the MCP client; `docker restart discord-mcp` plus an MCP reconnect (`/mcp` in Claude Code) brings it back. Full operational reference, restart procedure, channel discipline, and bot-side intent requirements: read `docs/guides/discord-mcp-server.md`.

---

## Design Principles

- **KISS**: Simplest solution that works. Share design choices so user decides on complexity.
- **YAGNI**: No features for hypothetical future requirements.
- **Minimal change surface**: Small, focused changes. Each change needs a concrete justification.
- **Comments about the present only**: Avoid defensive comments about hypothetical future scenarios ("if mode X is ever added, revisit this"). They confuse future readers by implying a plan that doesn't exist. Write only about what is true now. If the future concern is real, it belongs in a backlog task, not a code comment.
- **Fix the doc, not the identifier**: When an existing identifier's name is semantically correct but its documentation is wrong (stale docstring, mismatched comment, outdated reference), prefer fixing the documentation over renaming. A rename touching N call sites is rarely a net win when the name itself isn't the bug.

---

## Project Overview

ESP8266/ESP32 firmware for the NodoShop OpenTherm Gateway. Web UI, MQTT, REST API, TCP serial bridge, Home Assistant integration.

- **Platform**: ESP8266 (NodeMCU/Wemos D1 mini), ~40KB usable RAM; ESP32-S3 (OTGW32)
- **Language**: Arduino C/C++ (.ino files), single translation unit
- **Serial**: Reserved exclusively for PIC — never write to `Serial` after init
- **Debug**: `DebugTln()`, `DebugTf()` → Telnet port 23, never `Serial.print()`
- **Branches** (model changed 2026-06-20): **`dev` is now the DEFAULT working line and carries the 2.0.0 ESP32-S3-only async + FreeRTOS firmware** (epic TASK-865, ADR-123/128; ESP8266 dropped). It is the former `feature-2.0.0-esp32s3-async` line, promoted to `dev`. **`otgw-1.x.x` is the 1.5.x/1.6.x maintenance/LTS line** (the former `dev`, renamed). **`main` is ALWAYS the latest public release** (currently 1.6.1) and is never auto-pushed. The old `feature-2.0.0-esp32s3-async`, `feature-2.0.0-esp32s3-only` and `feature-dev-2.0.0-otgw32-esp32-sat-support` branches are historical/folded-in, not active. Default to the branch you are on; port fixes between `dev` (2.0.0) and `otgw-1.x.x` (1.x) deliberately, not reflexively.

---

## Critical Coding Rules

### PROGMEM — ALL string literals must stay in flash

```cpp
DebugTln(F("Message"));                        // F() macro
DebugTf(PSTR("Value: %d\r\n"), value);         // PSTR() for printf
snprintf_P(buf, size, PSTR("fmt: %s"), str);   // snprintf_P always
```

String comparisons: `strcmp_P()`, `strcasecmp_P()` with `PSTR()`.
Binary data: `memcmp_P()` only — **never** `strncmp_P`/`strstr_P` on binary (causes Exception (2) crash).

### No ArduinoJson — ever

JSON built manually with `snprintf_P` / `sendJsonMapEntry`. Parsed with `parseJsonKVLine()`.

### No String class in hot paths (ADR-004)

Use `char[]` with `strlcpy`, `snprintf_P`. `String` only in setup/one-off contexts.

### File serving — stream, never load into RAM

Files >2 KB: `httpServer.streamFile()`. `index.html` is ~11 KB — never `f.readString()`.

### HTTP/WS only — never add HTTPS/WSS

Trusted LAN only. REST API works behind HTTPS reverse proxy, but WebSocket assumes plain WS.

### ESP Platform Abstraction — no raw `#ifdef ESP8266`/`ESP32` outside the abstraction layer

The 2.0.0 branch carries an explicit platform abstraction so that
application code is written against shims and capability flags, never
against raw platform symbols. **No `#if(def) ESP8266`, `#if(def) ESP32`,
`#if(def) ARDUINO_ARCH_ESP*`, or `#if(def) BOARD_NODOSHOP_ESP*` may appear
outside the allowlisted abstraction files.**

Allowlisted files (the only place these conditionals belong):

- `src/OTGW-firmware/platform.h` — dispatcher
- `src/OTGW-firmware/platform_esp8266.h` — ESP8266 includes, shims, type aliases
- `src/OTGW-firmware/platform_esp32.h` — ESP32 includes, shims, type aliases
- `src/OTGW-firmware/boards.h` — pin maps and `HAS_*` capability flags
- `src/OTGW-firmware/OTGW-ModUpdateServer{.h,-esp32.h,-impl.h}` — parallel mini-abstraction for the firmware update server

**Vendored libraries under `src/libraries/**` are OUT OF SCOPE entirely.**
They are independent upstreams that manage their own platform support and are
NOT firmware application code. Do NOT pull them into the abstraction: never add
`platformXxx()` shims into them and never strip their internal
`#if defined(ESP8266)/(ESP32)`. Examples: `src/libraries/SimpleTelnet`,
`src/libraries/OTGWSerial` (PIC serial driver). They are listed in
`evaluate.py::ESP_ABSTRACTION_EXCLUDED_LIB_DIRS` and excluded from the
abstraction scan. (Maintainer directive, 2026-05-30.)

Application code MUST instead:

1. **Call `platformXxx()` shims** from `platform_*.h` for any divergent API
   (heap, hostname, MAC, reset, NTP, LED, JSON tx, WiFi, BLE, …). If a
   shim does not exist yet for a divergence you need, *add the shim first*
   in both `platform_esp8266.h` and `platform_esp32.h` — including an
   inline no-op stub on the platform where the feature is absent — and
   then call it unguarded from application code.
2. **Gate optional features with `HAS_*` flags** from `boards.h`
   (`HAS_PIC`, `HAS_DIRECT_OT`, `HAS_ETH_CAPABLE`, `HAS_OLED_CAPABLE`,
   `HAS_SAT_BLE`, `HAS_WEATHER_FORECAST`, etc.). If your feature does not
   yet have a flag, add one to `boards.h` first.
3. **Never read raw board macros (`BOARD_NODOSHOP_ESP32`, …) outside
   `boards.h`.** Those decide which `HAS_*` flags are set; the rest of
   the firmware sees only the `HAS_*` flags.
4. **Never call `ESP.getXxx()` directly** when a `platformXxx()` shim
   already exists (`platformFreeHeap`, `platformMinFreeHeap`,
   `platformMaxFreeBlock`, `platformHeapFragmentation`, `platformRestart`,
   `platformChipId`, `platformFlashChip*`, etc.). Skipping the shim is a
   quieter form of the same leak — it bypasses the platform's substitution
   point.

When adding a new feature that diverges per platform, the **first** thing
to write is the shim or the `HAS_*` flag, not the application code that
needs it. The platform headers are the public API of the abstraction; the
.ino files are clients.

`evaluate.py::check_esp_abstraction_boundary()` enforces this rule with a
baseline-ratchet: it FAILs on any new violation above the recorded
`ESP_ABSTRACTION_BASELINE` and WARNs while any pre-existing violation
remains. The current baseline and remediation roadmap live in
`docs/audits/2026-05-28-esp-abstraction-leak-audit.md` (TASK-739) and the
tier tasks TASK-740..746. **Each tier task must lower
`ESP_ABSTRACTION_BASELINE` in `evaluate.py` as part of its Definition of
Done.**

### Architecture rules

- PIC commands: always `addOTWGcmdtoqueue()`, never direct serial write
- Timers: `DECLARE_TIMER_SEC()` / `DECLARE_TIMER_MS()` + `DUE()` (see `safeTimers.h`)
- `doBackgroundTasks()` can re-enter (called from inside `doAutoConfigure()`'s file-reading loop). Shared scratch state must declare an acquisition contract: see ADR-090 for the pattern (RAII or inUse-flag, both with fail-safe on contention)
- Typed control flow: `enum class` or numeric IDs, never string tokens as discriminators
- Frontend JS: Chrome/Firefox/Safari (latest + 2). Check element existence, try-catch JSON.parse, response.ok, .catch() on all async
- Webui assets live in `src/OTGW-firmware/data/` (`index.html`, `index.js`, `components.css`, `ds-tokens.css`, `graph.js`, `sat.js`, `sat-slider.js`, `theme-toggle.js`, `echarts-theme.js`) and ship as a LittleFS image; use `python build.py` (not `--firmware`) to rebuild them
- Log container contract: `.ot-log-content` has `white-space: pre` (components.css); `\n` is the line separator. Prefer `textContent` over `innerHTML` for plain text (skips HTML parser and per-line escape)

### Naming conventions

- Variables/functions: camelCase (`settingHostname`, `startWiFi`)
- Constants: UPPER_CASE (`CMSG_SIZE`)
- Global settings: `setting` prefix (`settingMqttBroker`)

---

## Settings & State Architecture (ADR-051)

- `OTGWSettings settings` — persistent, serialized to LittleFS
- `OTGWState state` — transient, never persisted
- Two-level sub-sections, Hungarian prefixes: `b`=bool, `s`=char[], `i`=int, `f`=float
- Access: `settings.mqtt.sBroker`, `state.otgw.bOnline`

---

## REST API

- `/api/v2/` — current. Dispatch table `kV2Routes[]` in `restAPI.ino`.
- Errors: `sendApiError(httpCode, F("message"))`

---

## ADR Guidelines

ADRs in `docs/adr/`. Read before changes to: architecture, NFRs, API contracts, new dependencies.

**Binding ADRs** (pattern-level, enforced by `evaluate.py` or tests — see ADR-080):
- **ADR-004**: No `String` in hot paths (SAT*, MQTTstuff, restAPI, OTGW-Core, OTDirect)
- **ADR-088**: MQTT status-burst windowing + post-burst cooldown (gated by `check_status_publishers_wrap_burst`, `check_status_burst_cooldown_bound`, `check_drip_consults_deferred` in evaluate.py)
- **ADR-089**: Heap tier-machine contract (amends ADR-030; gated by `check_heap_tier_thresholds_ordered`, `check_heap_fragmentation_promotion`, `check_heap_tier_entry_counters` in evaluate.py)
- **ADR-091**: Design-system class drift gate (gated by `check_design_system_drift` in evaluate.py; WARN for one release, TASK-480 promotes to FAIL)

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

Accepted ADRs are binding. To reverse: new ADR that supersedes old one.

Format: Status / Context / Decision / Consequences / Related

Create an ADR when: architecture changes, new/replaced dependency, API contract change, build tooling change.
Do NOT create for: refactors, bug fixes, minor features within existing patterns.

Per **ADR-080**, a new pattern-level ADR MUST either reference its CI gate (in `evaluate.py` or `tests/`) or be explicitly labeled guideline-level in its Status line. No more "binding on paper, unchecked in practice".

---

## Build Commands

Preferred wrapper (handles venv setup): `./build.sh` (macOS/Linux) or `build.bat` (Windows). Both invoke `build.py` underneath and build firmware + filesystem. Use a direct `python build.py` invocation only when the wrapper is unavailable.

```bash
./build.sh                   # Preferred — firmware + filesystem (handles venv)
python build.py              # Build firmware + filesystem, all three targets
python build.py --target esp32-classic   # One target: esp8266 | esp32 | esp32-classic
python build.py --firmware   # Firmware only (also the push-policy gate)
python build.py --clean      # Clean build
python evaluate.py           # Code quality check (PROGMEM, unsafe patterns)
python evaluate.py --quick   # Fast check
```

Three fixed build targets (ADR-126 — no runtime hardware detection):

| Target | Hardware | Asset name token |
|---|---|---|
| `esp8266` | OTGW Classic + Wemos D1 mini (PIC) | `esp8266` |
| `esp32` | OTGW32 / OT-Thing PCB (OTDirect) | `esp32-otgw32` |
| `esp32-classic` | OTGW Classic + LOLIN S3 Mini (PIC) | `esp32-classic` |

Asset naming: `OTGW-firmware-<token>-<semver>+<githash>-flash.zip` (plus
`.ino.bin`, `.littlefs.bin`, merged bins and `.elf`). Never run two builds
concurrently in one worktree — shared `.pio/build/` corrupts (bug-034 class);
wipe the affected `.pio/build/<env>` dir and rebuild solo to recover.

---

## Test automation (on-device / soak)

End-to-end loop for firmware changes that need on-device validation (e.g. the
heap-frag soak, TASK-934):

- **Build (canonical):** `python build.py --target esp32` (or `./build.sh` /
  `build.bat`) — builds firmware + filesystem and bootstraps its own venv, so it
  is immune to the host Python version.
- **Build (direct pio) gotcha:** `python -m platformio run -e esp32` fails if the
  host Python is 3.14+ (the espressif32 platform requires 3.10–3.13: *"Python
  version must be between 3.10 and 3.13"*). Invoke pio via its own penv instead:
  `~/.platformio/penv/Scripts/python.exe -m platformio run -e esp32`
  (firmware) and `... -e esp32 -t buildfs` (LittleFS).
- **Fresh worktree:** run `git submodule update --init --recursive` before the
  first build — git worktrees do NOT populate submodule working trees, so
  `SimpleTelnet` (AsyncSimpleTelnet.h) and `OpenTherm` are missing otherwise and
  the sketch fails with *"No such file or directory"*.
- **Static gates:** `python evaluate.py` (full) / `--quick`. Exit 0 = pass, 1 =
  FAIL, 2 = WARN>5. NOT run by build.py — run it separately. Gate unit tests:
  `python tests/test_evaluate.py` (stdlib unittest, no pytest).
- **Flash:** `flash_otgw.bat --board esp32` (auto-detects the S3 over USB
  VID/PID 303A:1001; `--port COMx` to force). esptool-only — works on any Python.
- **Load (fragmenting):** `python scripts/sat_boiler_emulator.py --host <ip>` for
  synthetic OT traffic; combine with concurrent Web UI polling + MQTT discovery
  republish to exercise heap pressure.
- **Capture / soak:** `scripts/capture-mqtt-debug.bat -DeviceHost <ip> -BrokerHost
  <ip> -DurationSeconds <N> -Topic "otgw-firmware/stats/#"` → one merged
  transcript (telnet debug + MQTT stream + browser devtools + crashlog + HTTP
  probes).
- **Heap soak (TASK-934):** watch `otgw-firmware/stats/min_max_block`,
  `/maxblock_lt2k…ge16k`, `/max_loop_gap_ms` and the gating counters
  (`/drip_slowmode`, `/mqtt_drops`, `/ws_drops`, `/enter_*`). Telnet `z` zeroes the
  watermark/histogram/counters for a fresh window (`min_free_heap` is the native
  ESP32 allocator watermark and is not resettable). Press `z` from a healthy heap:
  if reset mid-pressure, the tier-entry counters (`enter_*`) only resume counting
  after the heap recovers to HEALTHY and re-enters the tier (they share
  `getHeapHealth()`'s internal state, which the reset deliberately does not touch).

---

## Important Constraints

- Never write to `Serial` after OTGW init (it's the PIC serial link)
- Never flash PIC firmware over WiFi using OTmonitor (bricks the PIC)
- Never add HTTPS/WSS
- Always `addOTWGcmdtoqueue()` for OTGW commands
- Always validate buffer sizes before string operations
- Always `feedWatchDog()` in long-running loops

---

## Project Navigation: What to Read Before Touching Code

**Always start with** `docs/c4/c4-context.md` (system overview) and `docs/c4/c4-component.md` (component index).

| Scenario | Read before starting |
|---|---|
| Bug in MQTT publishing | `c4-component-integration-layer.md` + `docs/api/MQTT.md` + `c4-code-mqtt.md` |
| Bug in OT message parsing | `c4-component-opentherm-core.md` + OT spec v4.2 + `c4-code-otgw-core.md` |
| Bug in web UI | `c4-component-web-interface.md` + `c4-code-web-assets.md` |
| New REST endpoint | `c4-component-integration-layer.md` + `c4-code-rest-api.md` |
| New MQTT topic | `docs/api/MQTT.md` + `c4-code-mqtt.md` + relevant ADRs |
| Settings/config change | `c4-component-configuration-state.md` + ADR-051 + `c4-code-settings.md` |
| Network/WiFi/OTA change | `c4-component-network.md` + `c4-code-network.md` |
| SAT/BLE feature | `c4-component-smart-thermostat.md` + `other-projects/SAT-releases-thermo-nova/` |
| ESP32 port/feature | `c4-container.md` + `other-projects/OT-Thing-OTGW32/` + relevant ADRs |
| OpenTherm protocol/message IDs | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` |
| MQTT/REST/WebSocket API change | `docs/api/MQTT.md` + `docs/api/WEBSOCKET_FLOW.md` — breaking silently breaks HA integrations |
| Architecture change | `c4-context.md` + `c4-component.md` + all relevant ADRs + create new ADR |
| New dependency | Relevant ADRs + PROGMEM/RAM budget check + create ADR |

**C4 component files** (all in `docs/c4/`):

| File | Covers |
|---|---|
| `c4-component-opentherm-core.md` | OT protocol, OTGW-Core, OTDirect, PIC serial |
| `c4-component-integration-layer.md` | MQTT, REST API, WebSocket, HA auto-config |
| `c4-component-web-interface.md` | Web UI, FSexplorer, file serving |
| `c4-component-network.md` | WiFi, Ethernet, OTA, mDNS, NTP |
| `c4-component-smart-thermostat.md` | SAT, BLE, simulation mode |
| `c4-component-sensors-hardware.md` | Dallas, S0 pulse counter, OLED |
| `c4-component-configuration-state.md` | Settings persistence, OTGWSettings, OTGWState |

**Code-level docs** (in `docs/c4/`, one per source area): `mqtt`, `network`, `otdirect`, `otgw-core`, `rest-api`, `sat`, `sensors`, `settings`, `utilities`, `web-assets`.

**Reference implementations** in `other-projects/` — read-only, never copy verbatim:

| Directory | When to read |
|---|---|
| `OT-Thing-OTGW32/` | ESP32 port, OTDirect, Ethernet, BLE |
| `SAT-releases-thermo-nova/` | SAT subsystem, BLE protocol |
| `otgw-6.6/` | PIC command set, response timing |
| `otmonitor-6.6/` | How a well-tested client drives the PIC |

**If you cannot name the C4 component that owns the code you are about to change, stop and read c4-context.md first.**

---

## Debugging User-Reported Issues

When investigating a user-reported bug, behavioural deviation, or "this should work" complaint, follow the reference precedence in this order:

1. **Read the OpenTherm specification first.** `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` is the ground truth. The spec defines what the protocol *says* should happen. Anchor every investigation here before opening implementation code, regardless of how confident you feel about the topic.
2. **Then consult `other-projects/`.** Read `other-projects/README.md` and `other-projects/CLAUDE.md` before quoting any reference implementation. Those files document the precedence ladder inside that directory: Schelte Bron's projects (otgw, otmonitor, otgwmcu) for PIC and otmonitor protocol authority; the HA `opentherm_gw` component for user-perceived behaviour; `pyotgw` for the engine under HA; OT-Thing and SAT for alternative implementations.
3. **Never modify code under `other-projects/`.** It is upstream reference, read-only by convention. The full rule and rationale live in `other-projects/CLAUDE.md`.

**Spec wins on design decisions.** When the spec is explicit and unambiguous, our firmware matches the spec, even if a downstream client behaves differently.

**HA component behaviour matters for understanding the report.** Most user complaints in practice are "this differs from what Home Assistant shows me", not "this differs from the OpenTherm spec". Match the report against the HA component to triage; match the design against the spec to decide.

**When spec-correct and HA-perceived behaviour conflict**, capture the trade-off as an ADR (`docs/adr/`). Do not silently align with one or the other; the next maintainer needs to know we made the call deliberately.

## Git push policy

The default Claude Code instruction is "do not push without explicit user permission". For this project, the maintainer (Robert) has granted standing permission to push to **`origin/dev`** (the 2.0.0 async default line) and **`origin/otgw-1.x.x`** (the 1.x maintenance line) when it is logical to do so. Logical means: a clean working state, recent commits that are self-contained, and no pending review checkpoints.

Concrete rules that override the default "ask first":

- **`origin/dev`** push: allowed once a task is committed locally AND the build verifies (`python build.py` returns exit 0 for the relevant ESP32 target — `dev` is 2.0.0 ESP32-S3, no esp8266 target here) AND the evaluator is green (`python evaluate.py --quick` shows no new failures). Mention the push in the user-facing summary. **Docs-only commits** (`*.md`, `docs/**`, `backlog/**`, `.claude/**`) may skip both gates — they cannot affect firmware compilation.
- **`origin/otgw-1.x.x`** push: allowed under the same conditions as `origin/dev` (task committed locally, build green for the esp8266 target, evaluator green; docs-only commits skip both gates). This is the 1.5.x/1.6.x maintenance/LTS line (the former `dev`).
- **`origin/main`** push: **`main` is ALWAYS the latest public release.** Never auto-pushed; every push requires explicit per-instance confirmation and is part of the release flow only.
- **Force-push** to any branch: still requires explicit per-instance confirmation. Force-push to main is forbidden regardless.
- **Sub-branches / other remote branches** (`feature-*`, `fix-*`, ad-hoc lanes): require explicit per-instance confirmation. Create a parallel lane only for genuine parallel work and delete it immediately after merging back (one-line policy, see Worktree layout).

When in doubt about whether a push is "logical", err toward asking. The cost of one extra prompt is small; the cost of an unwanted force-push is large.

## Versioning policy

Field-test users on Discord identify which build they are running by the prerelease tag in `_VERSION_PRERELEASE` (e.g. `alpha.6`, `beta.23`). Every commit that changes firmware behaviour must ship under its own prerelease tag so a tester can A/B two builds and report which one regressed. Implicit batching ("multiple fixes shipped under one tag") destroys that signal — so the policy is enforced mechanically.

**What requires a prerelease bump** — any staged path under:

- `src/OTGW-firmware/**` (the `.ino`/`.h`/`.cpp` sources and the LittleFS data assets), **except** `src/OTGW-firmware/version.h` itself;
- `src/libraries/**` (vendored libs that link into the firmware).

**What does NOT require a bump** (commit can land with no version change):

- `*.md`, `docs/**`, `backlog/**`, `.claude/**` — pure documentation / project metadata;
- `scripts/**`, `bin/**`, `.githooks/**` — host-side tooling not flashed to the device;
- top-level `.py` / `.sh` / `.bat` — build orchestration, not firmware.

**How to bump.** From the project root:

```bash
bin/bump-prerelease.sh        # bumps tag, syncs ALL version banners, stages everything it touched
git commit -m "..."
```

The bump script updates `version.h`, `data/version.hash`, **and every
source-file version banner** (via `autoinc-semver.py --update-all`), then
stages all of them itself. Do NOT hand-stage only `version.h` + `version.hash`
— that leaves the ~43 banner updates uncommitted and the headers drift out of
sync with the version file (observed: banners at alpha.165 while version.h was
at alpha.170). The whole bump lands in one commit, banner churn included.

The helper requires the current tag to match `^[a-zA-Z]+\.[0-9]+$` (so `alpha.6` → `alpha.7`, `beta.23` → `beta.24`). If you need to change the alpha/beta word itself, edit `version.h` by hand and let `scripts/autoinc-semver.py` handle the rest.

**Enforcement.** `.githooks/pre-commit` checks the staged diff. If any triggering path is staged but `_VERSION_PRERELEASE` is unchanged, the commit is rejected with a clear error. The hook is wired via `core.hooksPath = .githooks` (already set on this worktree).

**Bypass for one commit.** Use sparingly — e.g. an emergency rollback, or a commit that only re-formats whitespace and genuinely does not need its own tag:

```bash
OTGW_BUMP_HOOK_DISABLE=1 git commit -m "..."
```

If you find yourself reaching for the bypass routinely, the rule is wrong, not the commit. Open a backlog task to revisit the policy.

## Worktree layout

Branch model changed 2026-06-20. The 2.0.0 async line was promoted to `dev` (it is now the default working line), the old 1.x `dev` was renamed `otgw-1.x.x`, and `main` is always the latest public release. The everyday state is a **single working tree on `dev`**; a second worktree is only needed when actively maintaining the 1.x line. **The paths below assume the Windows root `D:/Users/Robert/Documents/GitHub/RvdB/` (Mac-canonical equivalent: `~/Library/CloudStorage/OneDrive-Belastingdienst/Documenten/GitHub/...`).**

| Worktree dir | Branch | Purpose |
|---|---|---|
| `.../GitHub/RvdB/OTGW-firmware` | `dev` | **DEFAULT working tree.** 2.0.0 ESP32-S3-only async + FreeRTOS line (epic TASK-865, ADR-123/128; ESP8266 dropped). |
| (add on demand) | `otgw-1.x.x` | 1.5.x/1.6.x maintenance/LTS line (the former `dev`). Spin up a worktree only when patching 1.x. |
| (release only) | `main` | ALWAYS the latest public release. Never a working tree; touched only during the release flow. |

The old `feature-2.0.0-esp32s3-async`, `feature-2.0.0-esp32s3-only` and `feature-dev-2.0.0-otgw32-esp32-sat-support` branches are historical (folded into `dev` / parked) and are not active work lines.

**One line, no stray sub-branches (maintainer directive, 2026-06-19, still in force).** Single-lane work commits and pushes DIRECTLY on `dev`, NO sub-branch. Create a sub-branch/worktree ONLY for a genuine parallel lane, and DELETE it immediately after merging back: `git branch -d <sub>`, `git push origin --delete <sub>`, `git worktree remove <dir>`. Codified in `.claude/skills/implement-next-task/SKILL.md`.

**Rule: don't cross lines inside one tree.** Work targeting `otgw-1.x.x` belongs in its own worktree. Never `git checkout otgw-1.x.x` inside the `dev` tree to do 1.x work: it defeats the split and risks losing in-flight changes.

**Windows gotcha:** `git worktree remove` can fail with "Function not implemented" (ENOSYS) when a worktree holds internal symlinks (venv `lib64`, toolchain). Remove junction-safe with `cmd //c rmdir /s /q "<path>"` then `git worktree prune`, never a blind recursive delete that may follow a junction's target.

Verify with `git worktree list`.

**Backlog.md: always use the CLI, never the MCP server.** The `backlog` MCP server (`mcp__backlog__task_*` tools) inherits the launching session's working directory and indexes only that single worktree. Tasks living in a sibling worktree are invisible to `mcp__backlog__task_search`, and `mcp__backlog__task_view` returns cached/stale content for cross-tree tasks (verified 2026-05-05: MCP kept returning the pre-edit "In Progress" snapshot of TASK-514 long after a CLI edit had marked it Done on disk in the 2.0.0 tree). Mixing CLI and MCP on the same task is fragile because MCP caches and does not reflect CLI-side writes without a server restart. **Use `backlog task ...` CLI for every read, edit, create, complete, and archive in this project.**

**CLI cross-tree behaviour.** The backlog now lives in the `dev` tree's `backlog/tasks/` — all 2.0.0 (TASK-865.x and friends) and legacy task files are here. If you spin up a second worktree for `otgw-1.x.x`, remember `backlog task edit` only writes to the worktree where the task file actually lives: if an edit returns `Task not found`, `find` for `task-<id>*` across both worktrees and run the edit from the worktree that holds the file.

### One worktree per concurrent session — never share a working tree between two live sessions

Two Claude (or Codex / Gemini) sessions doing git operations in the **same** worktree share one git index and one set of working-tree files. This is fragile and was observed to fail (2026-05-29):

- **Shared-index corruption.** Concurrent `git add`/commit from two sessions corrupts the index: `error: invalid object <hash> for '.claude-plugin/marketplace.json'` / `.gemini/settings.json`, then `error: Error building trees` — every commit is blocked. Recovery: `git reset` (mixed, no args) rebuilds the index from HEAD without touching commits or the working tree, then re-stage only your files and commit. A targeted `git reset -- <path>` only fixes one entry at a time (whack-a-mole when several are corrupt).
- **`.pio` build collisions.** Two `python build.py` runs in the same tree clobber `.pio/build/`, producing a spurious ESP32 LTO link failure (`undefined reference to setup()/loop()`) **even though `build.py` exits 0** (it does not propagate the per-env PlatformIO failure). Recovery: `rm -rf .pio/build/esp32` and rebuild solo. A background agent that backgrounds its own build and then ends leaves an orphaned build that collides with the next one — have agents NOT build, and let the main thread build once after they finish.
- **EOL churn is normal, not work.** On Windows every git touch flags `LF will be replaced by CRLF` on ~90 files that show as modified with zero content change. `git diff --ignore-cr-at-eol` reveals the real set; after committing real work, `git checkout -- .` restores the EOL-only files to leave a clean tree.

**Prevention.** Give each concurrent session its OWN worktree (`git worktree add ../wt-<topic> <branch-or-HEAD>`), OR run parallel work as **background sub-agents inside ONE session** (they serialise git through the single main thread, and the convention here is that sub-agents implement + report but do NOT commit/bump/build — the main thread does that once after reviewing). Do not run two top-level sessions in the same tree expecting clean commits.

### Cross-worktree work — ask first, then plan once, then parallelise

Whenever you take on a bug fix, feature change, or architectural decision, **explicitly ask yourself**: *does this also need to land on the other line?* For `dev` (2.0.0) ↔ `otgw-1.x.x` (1.x) the answer is **yes** when the change touches:

- Any file under `src/OTGW-firmware/` whose name is the same on both lines (most `.ino`, `.h`, `.cpp` and the LittleFS data assets) — even when the line numbers or filenames differ between branches (e.g. 1.x's `mqtt_configuratie.cpp` is `MQTTHaDiscovery.cpp` on 2.0.0), the architectural fix usually applies to both.
- ADRs that codify a cross-cutting decision (MQTT topic shape, heap policy, discovery semantics, settings schema). The `dev` ADR and the `otgw-1.x.x` ADR have separate numbering, but the *decision* must be coherent across both.
- Anything driven by a HA-side, broker-side or PIC-side contract (HA discovery regex, MQTT retained behaviour, OpenTherm message ID semantics) — those are platform-independent and the firmware-side fix must apply on both lines.

The answer is usually **no** when the change is genuinely scoped to a feature that only exists on one line (SAT dashboard, ESP32-S3 board pinning, OTGW32 hardware bring-up, async/FreeRTOS → `dev`/2.0.0 only; 1.x-specific patch backports → `otgw-1.x.x` only). Note: with 1.x now in maintenance/LTS, most active work is `dev`-only and cross-line porting is the exception, not the default.

**If both: one master plan, two tasks, two agents.**

1. **Write ONE master plan first**, before creating any task or spawning any agent. The plan covers *both* worktrees in a single document and must contain, for each change:
   - The desired outcome in plain language (the "what" and "why").
   - The exact file(s) and line number(s) on **both** branches — they often differ (dev refactor split files differently than 2.0.0). Read each branch's source to confirm; do not assume parity.
   - Per-platform considerations explicitly called out for the 2.0.0 side (ESP8266 vs ESP32-S3 deadbands/thresholds, board-specific pin maps, conditional compilation flags `#if defined(ESP32)` etc.).
   - The full AC list each per-worktree task will inherit (build commands per target, evaluator commands, field-validation gates, ADR-acceptance gates).
   - Cross-tree dependencies and ordering — e.g. dev ADR-N must be Accepted before 2.0.0 ADR-M can be drafted; dev impl can be pushed before 2.0.0 impl is reviewed.
   - The expected commit message prefix and the push gate per branch (`origin/dev` auto-push allowed under the dev worktree's policy; 2.0.0 feature branch push needs explicit confirmation).

2. **Share the master plan with the user for approval** before any task creation. The user's "go" on the master plan replaces the per-task plan-review gate that would otherwise apply individually.

3. **Then create two separate backlog tasks**, one per worktree. Use the convention `feat-2.0.0: port TASK-N — <title>` for the 2.0.0 sibling so cross-references stay legible. Each task carries its own AC list derived from the master plan; do not over-share ACs across tasks (each agent must be able to verify its own task in isolation).

4. **Spawn two agents in parallel**, one per worktree. Each agent:
   - Has its own self-contained prompt referencing the master plan (or restating its scope in full).
   - Works exclusively in its own worktree and explicitly does not touch the other tree.
   - Reports back independently with build/evaluator/commit/push receipts.

5. **Verify both reports**, then report a consolidated summary to the user.

Do **not** sequence (dev first, then 2.0.0 only after dev is fully done) unless there's a hard ordering dependency (e.g. 2.0.0 ADR cites a regex finding from the dev ADR — in that case dev ADR must be Accepted before the 2.0.0 ADR can be drafted, but the 2.0.0 *code* impl can still parallelise once both ADRs are Accepted).

The benefit of writing the plan once is symmetry: agents see the same intent, the two ADRs cross-reference cleanly, the two commits land within minutes of each other, and the user reviews one design instead of two slightly-divergent designs.

## ADR Kit Rules

This project uses [adr-kit](https://github.com/rvdbreemen/adr-kit) for Architecture Decision Records. The skill, the `adr-generator` subagent, and the path-specific instructions are loaded via the plugin.

- Use `/adr-kit:adr` (or invoke the `adr-generator` subagent) when authoring a new ADR.
- During coding work, follow the rules in the plugin's `instructions/adr.coding.md`.
- During code review, apply the six named ADR checks in the plugin's `instructions/adr.review.md`.
- An ADR cannot flip from `Proposed` to `Accepted` until it passes all four verification gates: Completeness, Evidence, Clarity, Consistency.

ADR files live at `docs/adr/ADR-XXX-title.md`. Status flow: `Proposed` to `Accepted`, then immutable. To reverse a decision, write a superseding ADR rather than editing the original.
