# Review Scope

## Target

Session work landed on `feature-dev-2.0.0-otgw32-esp32-sat-support` between
2026-04-30 00:25 and 02:55 UTC: 8 commits in the range
`ace21a48..fa5ef3c5`. Net diff vs the prior HEAD `c34d338d`:

- 12 source files changed
- 1 new ADR (`docs/adr/ADR-092-*`)
- 9 new backlog tasks (TASK-431, 432, 484, 486, 487, 488, 489, 490, 491,
  492; plus 4 archived: 384, 386, 430, 485)

## Commits in scope

| SHA | Subject | Type |
|---|---|---|
| `ace21a48` | TASK-482: evaluate.py PROGMEM gate fix | tooling |
| `759e47f8` | TASK-466: OTDirect TT/TC PIC remote-override + MsgID 100 | feat |
| `59b1478d` | TASK-487 + TASK-488: NimBLE port + HA auto-discovery | feat |
| `21aff0dd` | Backlog triage cleanup | chore |
| `d8028b0a` | Status updates, TASK-466 Done | chore |
| `67ad53cf` | TASK-489 + 490 + 491: code-review follow-ups | fix |
| `3e14c2cf` | Status updates, Done-flips | chore |
| `fa5ef3c5` | TASK-492: bleMacToCompact consolidation, dead-code cleanup | refactor |

## Files in review

- `evaluate.py` — PROGMEM gate macro-continuation tracking and `*.ino.cpp` exclusion
- `tests/test_evaluate.py` — new tests for the gate
- `src/OTGW-firmware/OTDirect.ino` — TT/TC remote-override state machine, MsgID 100 sync, sign-extend cast
- `src/OTGW-firmware/OTDirecttypes.h` — `OTRemoteOverrideMode` enum + `OTRemoteOverrideState` struct
- `src/OTGW-firmware/SATble.ino` — NimBLE 2.x rewrite, multi-sensor publish wiring, slot-recycling reset, dead-helper removal
- `src/OTGW-firmware/MQTTstuff.ino` — BLE HA-discovery helpers, `bleMacToCompact` exported
- `src/OTGW-firmware/OTGW-firmware.h` — forward declarations for new BLE helpers
- `platformio.ini` — `h2zero/NimBLE-Arduino @ ^2.1.0` added to `[env:esp32]` lib_deps
- `docs/adr/ADR-092-adopt-nimble-arduino-over-bluedroid-for-sat-ble-scanner.md` — new ADR Status=Accepted
- `tests/otdirect_pic_parity_fixture.md` — 9 new fixture rows for TASK-466 + TASK-491

## Out of scope

- Backlog markdown changes (task descriptions, status flips, archived tasks): metadata-only, no code impact.
- The 5 prior commits in `git log -10` (TASK-470, TASK-481, TASK-477, etc.) were already in place before this session and have had their own review cycles.
- ESP8266 build deltas: not applicable, all session work is ESP32-only paths.

## Flags

- Security Focus: no
- Performance Critical: yes (BLE in hot path, OTDirect on-bus state machine, ESP32 flash budget at 95.8%)
- Strict Mode: no
- Framework: Arduino/ESP8266+ESP32 (PlatformIO)

## Project rules and constraints

The project's CLAUDE.md (root) defines binding rules that the review should
verify compliance against:

- **PROGMEM**: All string literals must use `F()` / `PSTR()`. `snprintf_P`, `strcmp_P`, etc.
- **No ArduinoJson**: JSON built manually via `snprintf_P`.
- **No `String` in hot paths** (ADR-004): SAT, MQTTstuff, restAPI, OTGW-Core, OTDirect.
- **PIC serial reserved**: never write to `Serial` after OTGW init.
- **Debug to telnet only**: `DebugTln()` / `DebugTf()`.
- **No HTTPS/WSS**: trusted LAN only.
- **PIC commands via queue**: `addOTWGcmdtoqueue()`.
- **Settings/State architecture** (ADR-051): two-level structs, Hungarian prefixes.
- **Re-entrancy guard pattern** (ADR-090): RAII `MQTTAutoConfigSessionLock` or function-static `inUse` flag.
- **Streaming HA discovery** (ADR-077): chunked publish, drip-feed, no synchronous bursts.
- **Heap tier machine** (ADR-089): `canPublishMQTT()` and `MQTT_DISCOVERY_HEAP_MIN` gates.

## Review Phases

1. Code Quality & Architecture
2. Security & Performance
3. Testing & Documentation
4. Best Practices & Standards
5. Consolidated Report

## Output anchor

All findings cite file paths relative to repo root, with line numbers
where applicable. Severity scale: Critical / High / Medium / Low per the
skill specification.
