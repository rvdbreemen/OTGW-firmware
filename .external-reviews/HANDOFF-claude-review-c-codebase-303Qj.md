# Code Review Handoff — OTGW-firmware

**Reviewer:** Claude (Opus 4.7, 1M context), `claude/review-c-codebase-303Qj`
**Date:** 2026-04-21
**Scope:** Full C/C++ review of `src/OTGW-firmware/` (~24k LoC), architecture + quality + dead-code.

This document is a durable handoff. The review was produced in a single agentic session by three parallel specialist agents (architecture, quality, dead-code) and synthesized into the prioritized findings below. The findings exist here so they are not lost with the session.

---

## How to use this document

**Option A — Recommended: turn findings into backlog tasks, then work them one at a time.**

```bash
# 1. Make sure the Backlog.md CLI is installed — see https://backlog.md
backlog --version

# 2. Run the Python script (cross-platform; works on Windows, macOS, Linux)
python scripts/create_review_backlog.py --dry-run   # preview
python scripts/create_review_backlog.py             # actually create ~23 tasks

# 3. Pick up work the standard way
backlog task list --plain -s "To Do"
backlog task <id> --plain
backlog task edit <id> -s "In Progress" -a @claude
```

Each generated task cites the file:line from this document and has concrete acceptance criteria, so Claude Code can pick a task up cold.

**Option B — Hand issues to Claude Code ad-hoc.** Paste the relevant P0/P1/P2 section below into a Claude Code prompt and say "work this". Acceptable for one-off fixes, not great for the multi-file architectural items.

---

## Executive verdict

Grade: **C+**. Shippable, functional, but brittle under any refactor.

Three themes dominate everything below:

1. **Re-entrancy by convention, not by construction.** `doBackgroundTasks()` yields. `feedWatchDog()` yields. Several globals (`ot_log_buffer`, `cmdqueue[]`, static `sourceTopic`) are "safe" only because humans remember not to yield between the write and the consumer. This is the single biggest latent-stability class in the codebase.
2. **OTGW-Core.ino is a 4.7k-line god module.** OpenTherm decode, MQTT throttle arrays, WebSocket fan-out, REST timestamps, PIC state machine, command queue — all live in one file with no seams. Every change has high blast radius.
3. **ADR-051 (settings/state split) is half-applied.** Stragglers like `mqttPublishAllowed`, `statusBurstActive`, `verifyWildcard[]` are plain statics. Completing ADR-051 is a precondition for any module extraction.

Dead code is the least of the problems — maybe 50 lines to delete.

---

## P0 — Crash / stability / safety

| # | Finding | File:Line |
|---|---|---|
| **P0-1** | `ot_log_buffer[512]` is shared between `AddLog*` macros, Telnet, and WebSocket broadcast. `sendEventToWebSocket_P()` calls `feedWatchDog()` mid-flight. Re-entry via `doBackgroundTasks()` can corrupt the buffer. Ownership is documented as a comment, not enforced. | `OTGW-Core.ino:97`, `:3928–3930` |
| **P0-2** | `cmdqueue[]` / `cmdQueueSize` is mutated by 4+ callers (MQTT, REST, SAT, SATcycles) and by `processSerialTwo()` which yields. No guard. Under load, a command can be reordered or dropped. | `OTGW-Core.ino:~401` and every `addOTWGcmdtoqueue()` call site |
| **P0-3** | `static char sourceTopic[MQTT_TOPIC_MAX_LEN]` is filled with `snprintf()` then passed to `sendMQTTData()`, which yields. Assumption that nothing else rewrites the buffer is enforced by convention only. | `MQTTstuff.ino:1224–1228` |
| **P0-4** | `FSexplorer.ino:132` checks `strstr_P(lineBuf, PSTR(...))` then separately calls `strstr(lineBuf, "...")` on line 133 and dereferences the result (`*pos = '\0'`) **with no null check**. The two searches can disagree; result is a hard crash on an edge-case `index.html`. | `FSexplorer.ino:132–146` |
| **P0-5** | `SATcontrol.ino` bit-bangs OT status with magic masks (`& 0x08`). OTGW-Core has enum-based parsers elsewhere. A one-bit mistake silently changes control behavior. | `SATcontrol.ino:257, 406, 593` |

---

## P1 — Correctness / design

| # | Finding | File:Line |
|---|---|---|
| **P1-1** | `OTGW-Core.ino` mixes OT decode, MQTT throttle arrays, WebSocket dispatch, REST update timestamps, PIC-settings state machine, command queue — 4739 lines, no seams. | `OTGW-Core.ino` (entire) |
| **P1-2** | ADR-051 incomplete: `mqttPublishAllowed`, `statusBurstActive`, `statusBurstPublishCount`, `verifyActive`, `verifyWildcard[128]` are plain file-statics, not `state.mqtt.*` members. | `MQTTstuff.ino:60–184` |
| **P1-3** | REST API reads `OTcurrentSystemState` directly, bypassing the MQTT throttle/gate logic. Two presenters, two truths. | `restAPI.ino` (multiple) |
| **P1-4** | SAT publishes `TSet` via `satPublishMQTT()` in the same tick OTGW also publishes `TSet`. No de-dup — the same topic emits two conflicting messages under burst. | `SATcontrol.ino:~1730`; `MQTTstuff.ino` |
| **P1-5** | `String dBmtoQuality()` returns `String` — hot-path heap churn (WiFi quality). | `helperStuff.ino:646–656` |
| **P1-6** | Three `String` allocations during PIC upgrade control path. | `OTGW-Core.ino:4668–4670` |
| **P1-7** | `httpServer.arg("v").c_str()` passed to `strcmp()` — pointer into a temporary. Works only because rvalue outlives the call statement; fragile. | `FSexplorer.ino:98, 187, 201` |
| **P1-8** | `EVALBOOLEAN(x)` macro evaluates `x` three times and lacks parens around `x` — silent side-effect bug. | `OTGW-firmware.h:94` |
| **P1-9** | Two independent JSON escapers: `escapeJsonStringTo()` (public) and `static jsonEscape()` (private). Identical logic, guaranteed to diverge. | `jsonStuff.ino:14` / `OTGW-Core.ino:4342` |
| **P1-10** | `strlcpy_P` inline redefined despite the header guard in `OTGW-firmware.h:22`. | `mqtt_configuratie.cpp:1665` |
| **P1-11** | `snprintf(buffer, ..., "%d", v)` with RAM format string where `snprintf_P(PSTR(...))` is expected. | `MQTTstuff.ino:1182, 1188, 1227` |
| **P1-12** | `strcmp(state.pic.sDeviceid, "unknown")` — literal in RAM instead of `strcmp_P(..., PSTR("unknown"))`. | `OTGW-Core.ino:4600, 4683` |

---

## P2 — Quality / maintenance

- Deprecated v0 endpoints still live past their stated v1.3.0 removal date: `/api/firmwarefilelist`, `/api/listfiles` — `FSexplorer.ino:252–253`; handlers at `:283, :384`.
- Unreachable gas-consumption block guarded by `if (minCons > 0 && maxCons > 0)` where both are hard-coded `0.0f` — `SATcontrol.ino:2116–2119`.
- `static bool webhookInitialized` is written once, never read — `webhook.ino:17`.
- `evaluate.py` (785 lines of quality tooling) is **not wired into CI**. `.github/workflows/` has only two workflows, neither runs it.
- `tests/test_dallas_address.cpp` is orphaned — no Makefile/CMake/PlatformIO hook.
- Stale artifact directories: `.full-review/`, `.copilot-tracking/research/`.
- Commented-out debug lines scattered across: `helperStuff.ino:149–153, 331, 653`; `OTGW-firmware.ino:125, 183`; `restAPI.ino:760–761, 1076–1079`; `versionStuff.ino:25, 31, 41, 51–53`; `FSexplorer.ino:254`; `OTGW-Core.ino:2858`.
- Magic number `3` in `memcpy(value, buf+3, …)` — `OTGW-Core.ino:2814`. Needs `OT_CMD_VALUE_OFFSET`.
- Header `OTGW-firmware.h` pulls `Arduino.h`, `AceTime.h`, `SimpleTelnet.h`, `Wire.h`, `safeTimers.h`, `OTGWSerial.h`, `OTGW-Core.h`, `OneWire.h`, `DallasTemperature.h` into every `.ino` file — heavy preprocess cost per unit.

---

## Top-12 refactors, ranked by ROI

| Rank | Refactor | Effort | Payoff |
|---|---|---|---|
| 1 | Harden `ot_log_buffer` re-entrancy (P0-1) | S | Removes silent-corruption class |
| 2 | Extract MQTT throttle state into `state.mqtt` (P1-2) | M | Completes ADR-051; precondition for module split |
| 3 | Typed OT status accessors (P0-5) | S | Kills a bug class across SAT |
| 4 | Extract OT protocol from OTGW-Core.ino (P1-1) | L | Enables testability |
| 5 | Delete deprecated `/api/firmwarefilelist`, `/api/listfiles` (P2) | XS | Pure win |
| 6 | Collapse duplicate JSON escaper (P1-9) | XS | Removes drift risk |
| 7 | Replace `String dBmtoQuality()` with lookup table (P1-5) | XS | Hot-path heap removed |
| 8 | Fix `EVALBOOLEAN` macro (P1-8) | XS | Silent-bug prevention |
| 9 | Delete unreachable gas-consumption block (P2) | XS | Dead code |
| 10 | Remove `webhookInitialized` + commented-out debug lines (P2) | XS | Hygiene |
| 11 | Wire `evaluate.py` / `tests/` into CI or delete (P2) | S | Stops rot |
| 12 | Fix FSexplorer `strstr` null-deref (P0-4) | XS | Potential crash fix |

---

## Recommended sequencing

Do these in order — each unlocks the next:

1. **Crash-class first** — P0-1 (log buffer), P0-4 (null deref). Both are small, both are cheap, both are shippable as one PR.
2. **Complete ADR-051** — pull MQTT throttle flags into `state.mqtt` (P1-2). Until this is done, any attempt to split OTGW-Core.ino will leak globals across the new boundary.
3. **Typed OT status accessors** (P0-5, P1-4) — mechanical, touches many files, but trivially reviewable. Do before SAT refactor.
4. **Spring cleaning PR** — items 5, 6, 7, 8, 9, 10, 12 from the ranked table. All small, all independent, ship together.
5. **CI integration** — decide `evaluate.py` and `tests/` fate. Without CI there is no safety net for the next two items.
6. **Extract OT protocol from OTGW-Core.ino** (P1-1). Only after all of the above — this is the highest-risk change and must be done on a solid base.

---

## Creating the backlog tasks

The script at `scripts/create_review_backlog.py` creates ~23 backlog tasks (one per finding) with concrete acceptance criteria. Each task includes:

- Title with priority tag (P0/P1/P2 prefix).
- Description citing file:line from this document.
- `--ac` flags for verifiable outcomes.
- `--priority high|medium|low` matching the table above.
- `-l code-review,refactor,<category>` labels (`re-entrancy`, `architecture`, `quality`, `dedup`, `progmem`, `cleanup`).

**Usage:**

```bash
# Preview
python scripts/create_review_backlog.py --dry-run

# Create all
python scripts/create_review_backlog.py

# Create only one category
python scripts/create_review_backlog.py --only re-entrancy
```

**On Windows:** the script uses `subprocess` with list-args (not shell strings) and auto-locates `backlog`, `backlog.cmd`, or `backlog.exe` via `shutil.which`. No PowerShell quoting gymnastics.

After creation, verify with:

```bash
backlog task list --plain -l code-review
```

---

## Closing a task (reminder from CLAUDE.md §7)

A task is Done only when:

1. All acceptance criteria checked via `backlog task edit <id> --check-ac N`.
2. All Definition of Done items checked via `backlog task edit <id> --check-dod N`.
3. `--final-summary` set (acts as the PR description).
4. Status set: `backlog task edit <id> -s Done`.
5. Build + `python evaluate.py` pass.
6. No regressions.

**Do not edit task `.md` files by hand.** The CLI is the only supported interface (CLAUDE.md, top of file).

---

## Session artifacts

- Review branch: `claude/review-c-codebase-303Qj`
- This file: `HANDOFF.md`
- Task creation script: `scripts/create_review_backlog.py`
- Draft PR: created against `rvdbreemen/otgw-firmware` after push.
