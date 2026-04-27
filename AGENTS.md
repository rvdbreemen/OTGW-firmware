# OTGW-firmware: Codex Agent Instructions

These instructions adapt the repository's Claude guidance for Codex agents. They are project-specific rules for this firmware repository and sit above the generic Backlog.md CLI reference below.

---

## Task Management

Every meaningful code or documentation change should be backed by a Backlog task before implementation work starts. Use the Backlog.md CLI rules in the section below as the source of truth for reading, creating, updating, and completing tasks.

Codex agents must not edit files in `backlog/tasks/` directly. Use `backlog task create`, `backlog task edit`, `backlog task <id> --plain`, and `backlog search ... --plain`. If a Backlog MCP tool is explicitly available in the current session, it may be used for the same operations, but the CLI semantics below remain authoritative.

Before marking a task `Done`, run through `docs/guides/pr-checklist.md`. A clean build is the minimum bar; the checklist covers hardware, browser, MQTT, and smoke-test expectations that compile checks do not cover.

Known issue: `backlog task list` may return empty in this repository. Prefer `backlog search "<topic>" --plain` or `backlog task <id> --plain`. Read task files directly only as a read-only fallback, never to modify them.

---

## Design Principles

- KISS: choose the simplest solution that works and explain tradeoffs when complexity is optional.
- YAGNI: do not add behavior for hypothetical future needs.
- Minimal change surface: keep changes small, focused, and justified by the task.
- Comments describe the present system. Avoid defensive comments about hypothetical modes or future work; if the concern is real, create a Backlog task.
- Fix the documentation before renaming identifiers when the identifier is still semantically correct and only the comment or docstring is stale.

---

## Project Overview

This repository contains ESP8266 and ESP32 firmware for the NodoShop OpenTherm Gateway, including Web UI, MQTT, REST API, TCP serial bridge, Home Assistant integration, OTGW32 support, and SAT-related work.

- Platform: ESP8266 (NodeMCU/Wemos D1 mini, tight RAM budget) and ESP32/OTGW32.
- Language: Arduino C/C++ in `.ino` files, built as a single translation unit.
- Serial: reserved for the PIC after initialization. Never write to `Serial` after OTGW init.
- Debug: use `DebugTln()`, `DebugTf()`, and related debug helpers. They go to Telnet port 23; do not use `Serial.print()`.
- Branches: `dev` is the 1.4.x maintenance line. `feature-dev-2.0.0-otgw32-esp32-sat-support` is the 2.0.0 ESP32/OTGW32/SAT development line. Default to the current branch and port fixes deliberately.

---

## Critical Coding Rules

### PROGMEM strings

Keep string literals in flash wherever practical:

```cpp
DebugTln(F("Message"));
DebugTf(PSTR("Value: %d\r\n"), value);
snprintf_P(buf, size, PSTR("fmt: %s"), str);
```

Use `strcmp_P()` and `strcasecmp_P()` with `PSTR()` for flash-resident string comparisons. Use `memcmp_P()` for binary data; do not use `strncmp_P()` or `strstr_P()` on binary payloads.

### JSON

Do not add ArduinoJson. This firmware builds JSON manually with `snprintf_P()` / `sendJsonMapEntry()` and parses with `parseJsonKVLine()`.

### String usage

Avoid Arduino `String` in hot paths, especially areas covered by ADR-004: SAT, MQTT, REST, OTGW-Core, and OTDirect. Prefer fixed buffers with `strlcpy()` and `snprintf_P()`. `String` is acceptable only in setup or one-off paths where the local pattern already allows it.

### File serving

Stream files instead of loading them into RAM. Files larger than roughly 2 KB should use `httpServer.streamFile()`. `index.html` is too large for `readString()` style handling.

### HTTP and WebSocket

Do not add HTTPS or WSS support in firmware. This project targets trusted LAN deployment; REST can sit behind an HTTPS reverse proxy, while WebSocket assumptions remain plain WS.

### Architecture rules

- PIC commands must go through `addOTWGcmdtoqueue()`. Do not write command bytes directly to the PIC serial path.
- Timers should use `DECLARE_TIMER_SEC()` / `DECLARE_TIMER_MS()` plus `DUE()` from `safeTimers.h`.
- `doBackgroundTasks()` can re-enter, including while `doAutoConfigure()` is reading files. Shared scratch state needs an explicit acquisition contract, such as the ADR-090 RAII or in-use flag pattern.
- Prefer typed control flow with `enum class` or numeric IDs. Do not use string tokens as discriminators.
- Frontend JavaScript must support current Chrome, Firefox, and Safari plus two versions. Check element existence, use `try/catch` around `JSON.parse`, verify `response.ok`, and attach `.catch()` to async flows.
- Web UI assets live in `src/OTGW-firmware/data/` and ship as a LittleFS image. Use `python build.py`, not direct PlatformIO, when rebuilding firmware/filesystem artifacts.
- The log container contract is `.ot-log-content { white-space: pre; }`; `\n` is the line separator. Prefer `textContent` over `innerHTML` for plain log text.

### Naming conventions

- Variables and functions: camelCase, for example `settingHostname` and `startWiFi`.
- Constants: upper snake case, for example `CMSG_SIZE`.
- Persistent setting globals use the `setting` prefix when following existing legacy patterns.

---

## Settings and State Architecture

Follow ADR-051:

- `OTGWSettings settings` is persistent and serialized to LittleFS.
- `OTGWState state` is transient and must not be persisted.
- Settings/state use two-level subsections and Hungarian prefixes: `b` for bool, `s` for char arrays, `i` for integer, and `f` for float.
- Access should look like `settings.mqtt.sBroker` and `state.otgw.bOnline`.

---

## REST API

- `/api/v2/` is the current API surface.
- Dispatch lives in `kV2Routes[]` in `restAPI.ino`.
- Use `sendApiError(httpCode, F("message"))` for REST errors.

---

## ADR Guidelines

ADRs live in `docs/adr/`. Read relevant ADRs before architecture, NFR, API contract, dependency, build tooling, or shared pattern changes.

Binding ADRs with automated gates:

- ADR-004: no `String` in hot paths.
- ADR-088: MQTT status-burst windowing and post-burst cooldown.
- ADR-089: heap tier-machine contract.

Structural and architectural ADRs reviewed during PR:

- ADR-044: single point of instantiation for globals.
- ADR-051: settings/state architecture.
- ADR-056: protected admin endpoint security and secret-handling contract.
- ADR-077: streaming MQTT Home Assistant discovery architecture.
- ADR-078: MQTT sub-command dispatch tables.
- ADR-079: per-component type headers.
- ADR-080: binding ADR rules must have a CI gate.
- ADR-081: merge types into `<Component>stuff.h` when both exist.
- ADR-090: re-entrancy guard pattern for shared scratch buffers.

Accepted ADRs are binding. To reverse a decision, create a superseding ADR instead of editing the accepted ADR.

Create an ADR for architecture changes, new or replaced dependencies, API contract changes, and build tooling changes. Do not create ADRs for ordinary refactors, bug fixes, or small features that fit existing patterns.

New pattern-level ADRs must either reference their CI gate in `evaluate.py` or `tests/`, or explicitly mark themselves guideline-level in the status line.

This project also uses adr-kit. Use the repository's adr-kit workflow when authoring ADRs, follow its coding/review instructions during ADR-sensitive work, and keep ADRs in `docs/adr/ADR-XXX-title.md`. Status moves from `Proposed` to `Accepted`; accepted ADRs are immutable except through superseding ADRs.

---

## Build and Verification Commands

Use `build.py` rather than calling PlatformIO directly:

```bash
python build.py
python build.py --firmware
python build.py --clean
python evaluate.py
python evaluate.py --quick
```

Run the smallest verification that proves the change, then broaden when the change touches shared behavior, build tooling, network services, or public APIs.

---

## Important Constraints

- Never write to `Serial` after OTGW init; it is the PIC serial link.
- Never flash PIC firmware over WiFi using OTmonitor; that can brick the PIC.
- Never add HTTPS or WSS support.
- Always use `addOTWGcmdtoqueue()` for OTGW/PIC commands.
- Always validate buffer sizes before string operations.
- Always call `feedWatchDog()` in long-running loops.

---

## Project Navigation

Start with `docs/c4/c4-context.md` and `docs/c4/c4-component.md` when you do not already know the owning component.

Read these before touching specific areas:

| Scenario | Read before starting |
|---|---|
| MQTT publishing bug | `docs/c4/c4-component-integration-layer.md`, `docs/api/MQTT.md`, `docs/c4/c4-code-mqtt.md` |
| OpenTherm message parsing | `docs/c4/c4-component-opentherm-core.md`, OpenTherm spec v4.2, `docs/c4/c4-code-otgw-core.md` |
| Web UI bug | `docs/c4/c4-component-web-interface.md`, `docs/c4/c4-code-web-assets.md` |
| New REST endpoint | `docs/c4/c4-component-integration-layer.md`, `docs/c4/c4-code-rest-api.md` |
| New MQTT topic | `docs/api/MQTT.md`, `docs/c4/c4-code-mqtt.md`, relevant ADRs |
| Settings/config change | `docs/c4/c4-component-configuration-state.md`, ADR-051, `docs/c4/c4-code-settings.md` |
| Network/WiFi/OTA change | `docs/c4/c4-component-network.md`, `docs/c4/c4-code-network.md` |
| SAT/BLE feature | `docs/c4/c4-component-smart-thermostat.md`, `other-projects/SAT-releases-thermo-nova/` |
| ESP32 port/feature | `docs/c4/c4-container.md`, `other-projects/OT-Thing-OTGW32/`, relevant ADRs |
| OpenTherm protocol/message IDs | `docs/opentherm specification/OpenTherm-Protocol-Specification-v4.2.md` |
| MQTT/REST/WebSocket API change | `docs/api/MQTT.md`, `docs/api/WEBSOCKET_FLOW.md` |
| Architecture change | `docs/c4/c4-context.md`, `docs/c4/c4-component.md`, relevant ADRs, and a new ADR if needed |
| New dependency | Relevant ADRs, PROGMEM/RAM budget check, and a new ADR |

C4 component files in `docs/c4/`:

| File | Covers |
|---|---|
| `c4-component-opentherm-core.md` | OT protocol, OTGW-Core, OTDirect, PIC serial |
| `c4-component-integration-layer.md` | MQTT, REST API, WebSocket, Home Assistant auto-config |
| `c4-component-web-interface.md` | Web UI, FSexplorer, file serving |
| `c4-component-network.md` | WiFi, Ethernet, OTA, mDNS, NTP |
| `c4-component-smart-thermostat.md` | SAT, BLE, simulation mode |
| `c4-component-sensors-hardware.md` | Dallas, S0 pulse counter, OLED |
| `c4-component-configuration-state.md` | Settings persistence, `OTGWSettings`, `OTGWState` |

Code-level C4 docs are also in `docs/c4/` and cover MQTT, network, OTDirect, OTGW-Core, REST API, SAT, sensors, settings, utilities, and web assets.

Reference implementations in `other-projects/` are read-only references. Do not copy them verbatim:

| Directory | When to read |
|---|---|
| `other-projects/OT-Thing-OTGW32/` | ESP32 port, OTDirect, Ethernet, BLE |
| `other-projects/SAT-releases-thermo-nova/` | SAT subsystem and BLE protocol |
| `other-projects/otgw-6.6/` | PIC command set and response timing |
| `other-projects/otmonitor-6.6/` | How a mature client drives the PIC |

If you cannot name the C4 component that owns the code you are about to change, stop and read `docs/c4/c4-context.md` first.

---

<!-- BACKLOG.MD GUIDELINES START -->
# Instructions for the usage of Backlog.md CLI Tool

## Backlog.md: Comprehensive Project Management Tool via CLI

### Assistant Objective

Efficiently manage all project tasks, status, and documentation using the Backlog.md CLI, ensuring all project metadata
remains fully synchronized and up-to-date.

### Core Capabilities

- ✅ **Task Management**: Create, edit, assign, prioritize, and track tasks with full metadata
- ✅ **Search**: Fuzzy search across tasks, documents, and decisions with `backlog search`
- ✅ **Acceptance Criteria**: Granular control with add/remove/check/uncheck by index
- ✅ **Definition of Done checklists**: Per-task DoD items with add/remove/check/uncheck
- ✅ **Board Visualization**: Terminal-based Kanban board (`backlog board`) and web UI (`backlog browser`)
- ✅ **Git Integration**: Automatic tracking of task states across branches
- ✅ **Dependencies**: Task relationships and subtask hierarchies
- ✅ **Documentation & Decisions**: Structured docs and architectural decision records
- ✅ **Export & Reporting**: Generate markdown reports and board snapshots
- ✅ **AI-Optimized**: `--plain` flag provides clean text output for AI processing

### Why This Matters to You (AI Agent)

1. **Comprehensive system** - Full project management capabilities through CLI
2. **The CLI is the interface** - All operations go through `backlog` commands
3. **Unified interaction model** - You can use CLI for both reading (`backlog task 1 --plain`) and writing (
   `backlog task edit 1`)
4. **Metadata stays synchronized** - The CLI handles all the complex relationships

### Key Understanding

- **Tasks** live in `backlog/tasks/` as `task-<id> - <title>.md` files
- **You interact via CLI only**: `backlog task create`, `backlog task edit`, etc.
- **Use `--plain` flag** for AI-friendly output when viewing/listing
- **Never bypass the CLI** - It handles Git, metadata, file naming, and relationships

---

# ⚠️ CRITICAL: NEVER EDIT TASK FILES DIRECTLY. Edit Only via CLI

**ALL task operations MUST use the Backlog.md CLI commands**

- ✅ **DO**: Use `backlog task edit` and other CLI commands
- ✅ **DO**: Use `backlog task create` to create new tasks
- ✅ **DO**: Use `backlog task edit <id> --check-ac <index>` to mark acceptance criteria
- ❌ **DON'T**: Edit markdown files directly
- ❌ **DON'T**: Manually change checkboxes in files
- ❌ **DON'T**: Add or modify text in task files without using CLI

**Why?** Direct file editing breaks metadata synchronization, Git tracking, and task relationships.

---

## 1. Source of Truth & File Structure

### 📖 **UNDERSTANDING** (What you'll see when reading)

- Markdown task files live under **`backlog/tasks/`** (drafts under **`backlog/drafts/`**)
- Files are named: `task-<id> - <title>.md` (e.g., `task-42 - Add GraphQL resolver.md`)
- Project documentation is in **`backlog/docs/`**
- Project decisions are in **`backlog/decisions/`**

### 🔧 **ACTING** (How to change things)

- **All task operations MUST use the Backlog.md CLI tool**
- This ensures metadata is correctly updated and the project stays in sync
- **Always use `--plain` flag** when listing or viewing tasks for AI-friendly text output

---

## 2. Common Mistakes to Avoid

### ❌ **WRONG: Direct File Editing**

```markdown
# DON'T DO THIS:

1. Open backlog/tasks/task-7 - Feature.md in editor
2. Change "- [ ]" to "- [x]" manually
3. Add notes or final summary directly to the file
4. Save the file
```

### ✅ **CORRECT: Using CLI Commands**

```bash
# DO THIS INSTEAD:
backlog task edit 7 --check-ac 1  # Mark AC #1 as complete
backlog task edit 7 --notes "Implementation complete"  # Add notes
backlog task edit 7 --final-summary "PR-style summary"  # Add final summary
backlog task edit 7 -s "In Progress" -a @agent-k  # Multiple commands: change status and assign the task when you start working on the task
```

---

## 3. Understanding Task Format (Read-Only Reference)

⚠️ **FORMAT REFERENCE ONLY** - The following sections show what you'll SEE in task files.
**Never edit these directly! Use CLI commands to make changes.**

### Task Structure You'll See

```markdown
---
id: task-42
title: Add GraphQL resolver
status: To Do
assignee: [@sara]
labels: [backend, api]
---

## Description

Brief explanation of the task purpose.

## Acceptance Criteria

<!-- AC:BEGIN -->

- [ ] #1 First criterion
- [x] #2 Second criterion (completed)
- [ ] #3 Third criterion

<!-- AC:END -->

## Definition of Done

<!-- DOD:BEGIN -->

- [ ] #1 Tests pass
- [ ] #2 Docs updated

<!-- DOD:END -->

## Implementation Plan

1. Research approach
2. Implement solution

## Implementation Notes

Progress notes captured during implementation.

## Final Summary

PR-style summary of what was implemented.
```

### How to Modify Each Section

| What You Want to Change | CLI Command to Use                                       |
|-------------------------|----------------------------------------------------------|
| Title                   | `backlog task edit 42 -t "New Title"`                    |
| Status                  | `backlog task edit 42 -s "In Progress"`                  |
| Assignee                | `backlog task edit 42 -a @sara`                          |
| Labels                  | `backlog task edit 42 -l backend,api`                    |
| Description             | `backlog task edit 42 -d "New description"`              |
| Add AC                  | `backlog task edit 42 --ac "New criterion"`              |
| Add DoD                 | `backlog task edit 42 --dod "Ship notes"`                |
| Check AC #1             | `backlog task edit 42 --check-ac 1`                      |
| Check DoD #1            | `backlog task edit 42 --check-dod 1`                     |
| Uncheck AC #2           | `backlog task edit 42 --uncheck-ac 2`                    |
| Uncheck DoD #2          | `backlog task edit 42 --uncheck-dod 2`                   |
| Remove AC #3            | `backlog task edit 42 --remove-ac 3`                     |
| Remove DoD #3           | `backlog task edit 42 --remove-dod 3`                    |
| Add Plan                | `backlog task edit 42 --plan "1. Step one\n2. Step two"` |
| Add Notes (replace)     | `backlog task edit 42 --notes "What I did"`              |
| Append Notes            | `backlog task edit 42 --append-notes "Another note"` |
| Add Final Summary       | `backlog task edit 42 --final-summary "PR-style summary"` |
| Append Final Summary    | `backlog task edit 42 --append-final-summary "Another detail"` |
| Clear Final Summary     | `backlog task edit 42 --clear-final-summary` |

---

## 4. Defining Tasks

### Creating New Tasks

**Always use CLI to create tasks:**

```bash
# Example
backlog task create "Task title" -d "Description" --ac "First criterion" --ac "Second criterion"
```

### Title (one liner)

Use a clear brief title that summarizes the task.

### Description (The "why")

Provide a concise summary of the task purpose and its goal. Explains the context without implementation details.

### Acceptance Criteria (The "what")

**Understanding the Format:**

- Acceptance criteria appear as numbered checkboxes in the markdown files
- Format: `- [ ] #1 Criterion text` (unchecked) or `- [x] #1 Criterion text` (checked)

**Managing Acceptance Criteria via CLI:**

⚠️ **IMPORTANT: How AC Commands Work**

- **Adding criteria (`--ac`)** accepts multiple flags: `--ac "First" --ac "Second"` ✅
- **Checking/unchecking/removing** accept multiple flags too: `--check-ac 1 --check-ac 2` ✅
- **Mixed operations** work in a single command: `--check-ac 1 --uncheck-ac 2 --remove-ac 3` ✅

```bash
# Examples

# Add new criteria (MULTIPLE values allowed)
backlog task edit 42 --ac "User can login" --ac "Session persists"

# Check specific criteria by index (MULTIPLE values supported)
backlog task edit 42 --check-ac 1 --check-ac 2 --check-ac 3  # Check multiple ACs
# Or check them individually if you prefer:
backlog task edit 42 --check-ac 1    # Mark #1 as complete
backlog task edit 42 --check-ac 2    # Mark #2 as complete

# Mixed operations in single command
backlog task edit 42 --check-ac 1 --uncheck-ac 2 --remove-ac 3

# ❌ STILL WRONG - These formats don't work:
# backlog task edit 42 --check-ac 1,2,3  # No comma-separated values
# backlog task edit 42 --check-ac 1-3    # No ranges
# backlog task edit 42 --check 1         # Wrong flag name

# Multiple operations of same type
backlog task edit 42 --uncheck-ac 1 --uncheck-ac 2  # Uncheck multiple ACs
backlog task edit 42 --remove-ac 2 --remove-ac 4    # Remove multiple ACs (processed high-to-low)
```

### Definition of Done checklist (per-task)

Definition of Done items are a second checklist in each task. Defaults come from `definition_of_done` in `backlog/config.yml` (or Web UI Settings) and can be disabled per task.

**Managing Definition of Done via CLI:**

```bash
# Add DoD items (MULTIPLE values allowed)
backlog task edit 42 --dod "Run tests" --dod "Update docs"

# Check/uncheck DoD items by index (MULTIPLE values supported)
backlog task edit 42 --check-dod 1 --check-dod 2
backlog task edit 42 --uncheck-dod 1

# Remove DoD items by index
backlog task edit 42 --remove-dod 2

# Create without defaults
backlog task create "Feature" --no-dod-defaults
```

**Key Principles for Good ACs:**

- **Outcome-Oriented:** Focus on the result, not the method.
- **Testable/Verifiable:** Each criterion should be objectively testable
- **Clear and Concise:** Unambiguous language
- **Complete:** Collectively cover the task scope
- **User-Focused:** Frame from end-user or system behavior perspective

Good Examples:

- "User can successfully log in with valid credentials"
- "System processes 1000 requests per second without errors"
- "CLI preserves literal newlines in description/plan/notes/final summary; `\\n` sequences are not auto‑converted"

Bad Example (Implementation Step):

- "Add a new function handleLogin() in auth.ts"
- "Define expected behavior and document supported input patterns"

### Task Breakdown Strategy

1. Identify foundational components first
2. Create tasks in dependency order (foundations before features)
3. Ensure each task delivers value independently
4. Avoid creating tasks that block each other

### Task Requirements

- Tasks must be **atomic** and **testable** or **verifiable**
- Each task should represent a single unit of work for one PR
- **Never** reference future tasks (only tasks with id < current task id)
- Ensure tasks are **independent** and don't depend on future work

---

## 5. Implementing Tasks

### 5.1. First step when implementing a task

The very first things you must do when you take over a task are:

* set the task in progress
* assign it to yourself

```bash
# Example
backlog task edit 42 -s "In Progress" -a @{myself}
```

### 5.2. Review Task References and Documentation

Before planning, check if the task has any attached `references` or `documentation`:
- **References**: Related code files, GitHub issues, or URLs relevant to the implementation
- **Documentation**: Design docs, API specs, or other materials for understanding context

These are visible in the task view output. Review them to understand the full context before drafting your plan.

### 5.3. Create an Implementation Plan (The "how")

Previously created tasks contain the why and the what. Once you are familiar with that part you should think about a
plan on **HOW** to tackle the task and all its acceptance criteria. This is your **Implementation Plan**.
First do a quick check to see if all the tools that you are planning to use are available in the environment you are
working in.
When you are ready, write it down in the task so that you can refer to it later.

```bash
# Example
backlog task edit 42 --plan "1. Research codebase for references\n2Research on internet for similar cases\n3. Implement\n4. Test"
```

## 5.4. Implementation

Once you have a plan, you can start implementing the task. This is where you write code, run tests, and make sure
everything works as expected. Follow the acceptance criteria one by one and MARK THEM AS COMPLETE as soon as you
finish them.

### 5.5 Implementation Notes (Progress log)

Use Implementation Notes to log progress, decisions, and blockers as you work.
Append notes progressively during implementation using `--append-notes`:

```
backlog task edit 42 --append-notes "Investigated root cause" --append-notes "Added tests for edge case"
```

```bash
# Example
backlog task edit 42 --notes "Initial implementation done; pending integration tests"
```

### 5.6 Final Summary (PR description)

When you are done implementing a task you need to prepare a PR description for it.
Because you cannot create PRs directly, write the PR as a clean summary in the Final Summary field.

**Quality bar:** Write it like a reviewer will see it. A one‑liner is rarely enough unless the change is truly trivial.
Include the key scope so someone can understand the impact without reading the whole diff.

```bash
# Example
backlog task edit 42 --final-summary "Implemented pattern X because Reason Y; updated files Z and W; added tests"
```

**IMPORTANT**: Do NOT include an Implementation Plan when creating a task. The plan is added only after you start the
implementation.

- Creation phase: provide Title, Description, Acceptance Criteria, and optionally labels/priority/assignee.
- When you begin work, switch to edit, set the task in progress and assign to yourself
  `backlog task edit <id> -s "In Progress" -a "..."`.
- Think about how you would solve the task and add the plan: `backlog task edit <id> --plan "..."`.
- After updating the plan, share it with the user and ask for confirmation. Do not begin coding until the user approves the plan or explicitly tells you to skip the review.
- Append Implementation Notes during implementation using `--append-notes` as progress is made.
- Add Final Summary only after completing the work: `backlog task edit <id> --final-summary "..."` (replace) or append using `--append-final-summary`.

## Phase discipline: What goes where

- Creation: Title, Description, Acceptance Criteria, labels/priority/assignee.
- Implementation: Implementation Plan (after moving to In Progress and assigning to yourself) + Implementation Notes (progress log, appended as you work).
- Wrap-up: Final Summary (PR description), verify AC and Definition of Done checks.

**IMPORTANT**: Only implement what's in the Acceptance Criteria. If you need to do more, either:

1. Update the AC first: `backlog task edit 42 --ac "New requirement"`
2. Or create a new follow up task: `backlog task create "Additional feature"`

---

## 6. Typical Workflow

```bash
# 1. Identify work
backlog task list -s "To Do" --plain

# 2. Read task details
backlog task 42 --plain

# 3. Start work: assign yourself & change status
backlog task edit 42 -s "In Progress" -a @myself

# 4. Add implementation plan
backlog task edit 42 --plan "1. Analyze\n2. Refactor\n3. Test"

# 5. Share the plan with the user and wait for approval (do not write code yet)

# 6. Work on the task (write code, test, etc.)

# 7. Mark acceptance criteria as complete (supports multiple in one command)
backlog task edit 42 --check-ac 1 --check-ac 2 --check-ac 3  # Check all at once
# Or check them individually if preferred:
# backlog task edit 42 --check-ac 1
# backlog task edit 42 --check-ac 2
# backlog task edit 42 --check-ac 3

# 8. Add Final Summary (PR Description)
backlog task edit 42 --final-summary "Refactored using strategy pattern, updated tests"

# 9. Mark task as done
backlog task edit 42 -s Done
```

---

## 7. Definition of Done (DoD)

A task is **Done** only when **ALL** of the following are complete:

### ✅ Via CLI Commands:

1. **All acceptance criteria checked**: Use `backlog task edit <id> --check-ac <index>` for each
2. **All Definition of Done items checked**: Use `backlog task edit <id> --check-dod <index>` for each
3. **Final Summary added**: Use `backlog task edit <id> --final-summary "..."`
4. **Status set to Done**: Use `backlog task edit <id> -s Done`

### ✅ Via Code/Testing:

5. **Tests pass**: Run test suite and linting
6. **Documentation updated**: Update relevant docs if needed
7. **Code reviewed**: Self-review your changes
8. **No regressions**: Performance, security checks pass

⚠️ **NEVER mark a task as Done without completing ALL items above**

---

## 8. Finding Tasks and Content with Search

When users ask you to find tasks related to a topic, use the `backlog search` command with `--plain` flag:

```bash
# Search for tasks about authentication
backlog search "auth" --plain

# Search only in tasks (not docs/decisions)
backlog search "login" --type task --plain

# Search with filters
backlog search "api" --status "In Progress" --plain
backlog search "bug" --priority high --plain
```

**Key points:**
- Uses fuzzy matching - finds "authentication" when searching "auth"
- Searches task titles, descriptions, and content
- Also searches documents and decisions unless filtered with `--type task`
- Always use `--plain` flag for AI-readable output

---

## 9. Quick Reference: DO vs DON'T

### Viewing and Finding Tasks

| Task         | ✅ DO                        | ❌ DON'T                         |
|--------------|-----------------------------|---------------------------------|
| View task    | `backlog task 42 --plain`   | Open and read .md file directly |
| List tasks   | `backlog task list --plain` | Browse backlog/tasks folder     |
| Check status | `backlog task 42 --plain`   | Look at file content            |
| Find by topic| `backlog search "auth" --plain` | Manually grep through files |

### Modifying Tasks

| Task          | ✅ DO                                 | ❌ DON'T                           |
|---------------|--------------------------------------|-----------------------------------|
| Check AC      | `backlog task edit 42 --check-ac 1`  | Change `- [ ]` to `- [x]` in file |
| Add notes     | `backlog task edit 42 --notes "..."` | Type notes into .md file          |
| Add final summary | `backlog task edit 42 --final-summary "..."` | Type summary into .md file |
| Change status | `backlog task edit 42 -s Done`       | Edit status in frontmatter        |
| Add AC        | `backlog task edit 42 --ac "New"`    | Add `- [ ] New` to file           |

---

## 10. Complete CLI Command Reference

### Task Creation

| Action           | Command                                                                             |
|------------------|-------------------------------------------------------------------------------------|
| Create task      | `backlog task create "Title"`                                                       |
| With description | `backlog task create "Title" -d "Description"`                                      |
| With AC          | `backlog task create "Title" --ac "Criterion 1" --ac "Criterion 2"`                 |
| With final summary | `backlog task create "Title" --final-summary "PR-style summary"`                 |
| With references  | `backlog task create "Title" --ref src/api.ts --ref https://github.com/issue/123`   |
| With documentation | `backlog task create "Title" --doc https://design-docs.example.com`               |
| With all options | `backlog task create "Title" -d "Desc" -a @sara -s "To Do" -l auth --priority high --ref src/api.ts --doc docs/spec.md` |
| Create draft     | `backlog task create "Title" --draft`                                               |
| Create subtask   | `backlog task create "Title" -p 42`                                                 |

### Task Modification

| Action           | Command                                     |
|------------------|---------------------------------------------|
| Edit title       | `backlog task edit 42 -t "New Title"`       |
| Edit description | `backlog task edit 42 -d "New description"` |
| Change status    | `backlog task edit 42 -s "In Progress"`     |
| Assign           | `backlog task edit 42 -a @sara`             |
| Add labels       | `backlog task edit 42 -l backend,api`       |
| Set priority     | `backlog task edit 42 --priority high`      |

### Acceptance Criteria Management

| Action              | Command                                                                     |
|---------------------|-----------------------------------------------------------------------------|
| Add AC              | `backlog task edit 42 --ac "New criterion" --ac "Another"`                  |
| Remove AC #2        | `backlog task edit 42 --remove-ac 2`                                        |
| Remove multiple ACs | `backlog task edit 42 --remove-ac 2 --remove-ac 4`                          |
| Check AC #1         | `backlog task edit 42 --check-ac 1`                                         |
| Check multiple ACs  | `backlog task edit 42 --check-ac 1 --check-ac 3`                            |
| Uncheck AC #3       | `backlog task edit 42 --uncheck-ac 3`                                       |
| Mixed operations    | `backlog task edit 42 --check-ac 1 --uncheck-ac 2 --remove-ac 3 --ac "New"` |

### Task Content

| Action           | Command                                                  |
|------------------|----------------------------------------------------------|
| Add plan         | `backlog task edit 42 --plan "1. Step one\n2. Step two"` |
| Add notes        | `backlog task edit 42 --notes "Implementation details"`  |
| Add final summary | `backlog task edit 42 --final-summary "PR-style summary"` |
| Append final summary | `backlog task edit 42 --append-final-summary "More details"` |
| Clear final summary | `backlog task edit 42 --clear-final-summary` |
| Add dependencies | `backlog task edit 42 --dep task-1 --dep task-2`         |
| Add references   | `backlog task edit 42 --ref src/api.ts --ref https://github.com/issue/123` |
| Add documentation | `backlog task edit 42 --doc https://design-docs.example.com --doc docs/spec.md` |

### Multi‑line Input (Description/Plan/Notes/Final Summary)

The CLI preserves input literally. Shells do not convert `\n` inside normal quotes. Use one of the following to insert real newlines:

- Bash/Zsh (ANSI‑C quoting):
  - Description: `backlog task edit 42 --desc $'Line1\nLine2\n\nFinal'`
  - Plan: `backlog task edit 42 --plan $'1. A\n2. B'`
  - Notes: `backlog task edit 42 --notes $'Done A\nDoing B'`
  - Append notes: `backlog task edit 42 --append-notes $'Progress update line 1\nLine 2'`
  - Final summary: `backlog task edit 42 --final-summary $'Shipped A\nAdded B'`
  - Append final summary: `backlog task edit 42 --append-final-summary $'Added X\nAdded Y'`
- POSIX portable (printf):
  - `backlog task edit 42 --notes "$(printf 'Line1\nLine2')"`
- PowerShell (backtick n):
  - `backlog task edit 42 --notes "Line1`nLine2"`

Do not expect `"...\n..."` to become a newline. That passes the literal backslash + n to the CLI by design.

Descriptions support literal newlines; shell examples may show escaped `\\n`, but enter a single `\n` to create a newline.

### Implementation Notes Formatting

- Keep implementation notes concise and time-ordered; focus on progress, decisions, and blockers.
- Use short paragraphs or bullet lists instead of a single long line.
- Use Markdown bullets (`-` for unordered, `1.` for ordered) for readability.
- When using CLI flags like `--append-notes`, remember to include explicit
  newlines. Example:

  ```bash
  backlog task edit 42 --append-notes $'- Added new API endpoint\n- Updated tests\n- TODO: monitor staging deploy'
  ```

### Final Summary Formatting

- Treat the Final Summary as a PR description: lead with the outcome, then add key changes and tests.
- Keep it clean and structured so it can be pasted directly into GitHub.
- Prefer short paragraphs or bullet lists and avoid raw progress logs.
- Aim to cover: **what changed**, **why**, **user impact**, **tests run**, and **risks/follow‑ups** when relevant.
- Avoid single‑line summaries unless the change is truly tiny.

**Example (good, not rigid):**
```
Added Final Summary support across CLI/MCP/Web/TUI to separate PR summaries from progress notes.

Changes:
- Added `finalSummary` to task types and markdown section parsing/serialization (ordered after notes).
- CLI/MCP/Web/TUI now render and edit Final Summary; plain output includes it.

Tests:
- bun test src/test/final-summary.test.ts
- bun test src/test/cli-final-summary.test.ts
```

### Task Operations

| Action             | Command                                      |
|--------------------|----------------------------------------------|
| View task          | `backlog task 42 --plain`                    |
| List tasks         | `backlog task list --plain`                  |
| Search tasks       | `backlog search "topic" --plain`              |
| Search with filter | `backlog search "api" --status "To Do" --plain` |
| Filter by status   | `backlog task list -s "In Progress" --plain` |
| Filter by assignee | `backlog task list -a @sara --plain`         |
| Archive task       | `backlog task archive 42`                    |
| Demote to draft    | `backlog task demote 42`                     |

---

## Common Issues

| Problem              | Solution                                                           |
|----------------------|--------------------------------------------------------------------|
| Task not found       | Check task ID with `backlog task list --plain`                     |
| AC won't check       | Use correct index: `backlog task 42 --plain` to see AC numbers     |
| Changes not saving   | Ensure you're using CLI, not editing files                         |
| Metadata out of sync | Re-edit via CLI to fix: `backlog task edit 42 -s <current-status>` |

---

## Remember: The Golden Rule

**🎯 If you want to change ANYTHING in a task, use the `backlog task edit` command.**
**📖 Use CLI to read tasks, exceptionally READ task files directly, never WRITE to them.**

Full help available: `backlog --help`

<!-- BACKLOG.MD GUIDELINES END -->
