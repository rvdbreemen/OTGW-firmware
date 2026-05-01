# OpenWolf

@.wolf/OPENWOLF.md

This project uses OpenWolf for context management. Read and follow .wolf/OPENWOLF.md every session. Check .wolf/cerebrum.md before generating code. Check .wolf/anatomy.md before reading files.


# OTGW-firmware: Claude Instructions

---

## Task Management (MANDATORY)

**Every piece of work must have a backlog task before any code is written. No exceptions.**

**Preferred interface: MCP server (`mcp__backlog__*`) over the CLI.** MCP calls take structured parameters (typed arrays, multi-line strings) — no shell-escape pain. Use the CLI only when an operation is not exposed via MCP or the MCP server is unreachable. Coverage includes task create/edit/view/list/search/archive/complete, documents, milestones, Definition of Done defaults, and the workflow/execution guides. The CLI reference below remains the authoritative semantics description — same underlying engine.

```bash
# Before writing any code (CLI shown for reference; MCP equivalents preferred):
backlog search "<topic>" --plain           # 1. Find existing task       (mcp__backlog__task_search)
backlog task create "Title" -d "..." --ac "..."  # 2. Create if none     (mcp__backlog__task_create)
backlog task edit <id> -s "In Progress" -a @claude  # 3. Start it        (mcp__backlog__task_edit)
backlog task edit <id> --plan "..."        # 4. Write plan, share with user, WAIT for approval

# During implementation:
backlog task edit <id> --check-ac 1        # Mark ACs done as you go
backlog task edit <id> --append-notes "..."  # Log progress

# When done:
backlog task edit <id> --final-summary "..." # PR description
backlog task edit <id> -s Done
```

**CRITICAL: NEVER edit task files in `backlog/tasks/` directly. Always use the MCP server (preferred) or the CLI as fallback.**

Two hooks enforce this contract — they fail closed, you don't have to remember:

- `.claude/hooks/backlog-mcp-guard.py` — PreToolUse guard wired in `.claude/settings.json`. Blocks `Edit/Write/MultiEdit` on `backlog/tasks/*.md` and Bash invocations of the `backlog` CLI. Stderr names the MCP equivalent so the model self-corrects without another round-trip.
- `.githooks/commit-msg` — git hook that fails the commit if its message references `TASK-NNN` without a matching `backlog/tasks/task-NNN*.md` file in the index. Catches the failure mode where a code commit lands but its task record stays untracked. Install once per clone with `git config core.hooksPath .githooks`. Bypass with `git commit --no-verify` for emergencies (document why in the message).

For the full CLI reference (all commands, AC management, DoD, multi-line input): read `docs/guides/backlog-cli.md`.

Before marking a task `Done`, run through `docs/guides/pr-checklist.md`. "Builds clean" is the lowest bar; the checklist captures the hardware / browser / MQTT smoke tests that build-clean doesn't see.

**Known bug:** `backlog task list` returns empty. Use `backlog task <id> --plain` or read `backlog/tasks/` directly.

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
- **Branches**: `dev` is the 1.5.x maintenance line; `feature-dev-2.0.0-otgw32-esp32-sat-support` is the 2.0.0 development branch (ESP32/OTGW32 + SAT). Default to the branch you are on; port fixes deliberately, not reflexively.

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

```bash
python build.py              # Build firmware + filesystem
python build.py --firmware   # Firmware only
python build.py --clean      # Clean build
python evaluate.py           # Code quality check (PROGMEM, unsafe patterns)
python evaluate.py --quick   # Fast check
```

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

## ADR Kit Rules

This project uses [adr-kit](https://github.com/rvdbreemen/adr-kit) for Architecture Decision Records. The skill, the `adr-generator` subagent, and the path-specific instructions are loaded via the plugin.

- Use `/adr-kit:adr` (or invoke the `adr-generator` subagent) when authoring a new ADR.
- During coding work, follow the rules in the plugin's `instructions/adr.coding.md`.
- During code review, apply the six named ADR checks in the plugin's `instructions/adr.review.md`.
- An ADR cannot flip from `Proposed` to `Accepted` until it passes all four verification gates: Completeness, Evidence, Clarity, Consistency.

ADR files live at `docs/adr/ADR-XXX-title.md`. Status flow: `Proposed` to `Accepted`, then immutable. To reverse a decision, write a superseding ADR rather than editing the original.
