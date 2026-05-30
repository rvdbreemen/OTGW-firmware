---
id: ADR-115
title: Per-Board Numeric Constants and Typedefs Live in boards.h
status: Accepted
date: 2026-05-30
tags: [architecture, esp32, abstraction, portability, boards]
supersedes: []
superseded_by: []
related: [ADR-061, ADR-051, ADR-079]
deciders: [Robert van den Breemen]
---

# ADR-115: Per-Board Numeric Constants and Typedefs Live in boards.h

## Status

Accepted. Date: 2026-05-30.

**Decision Maker:** User: Robert van den Breemen (requested a new ADR for this Tier 3 choice).

## Status History

status_history:
  - date: 2026-05-30
    status: Accepted
    changed_by: User
    reason: Documents the boards.h home chosen in TASK-743 Tier 3a; implementation already landed (commit d4be6d4e, alpha.106) and builds green on both targets
    changed_via: adr-kit

## Context

ADR-061 established the unified ESP8266/ESP32 platform abstraction layer and
assigned homes for platform-divergent code:

- `platform.h` + `platform_esp8266.h` / `platform_esp32.h` — divergent **APIs**,
  wrapped behind `platformXxx()` shims and type aliases.
- `boards.h` — board-level **pin maps** and `HAS_*` capability flags.

ADR-061 did not say where a third category belongs: per-board **compile-time
numeric tuning constants** (buffer sizes, ring depths, heap floors, cooldown
windows) and the **integer typedefs** whose width depends on those sizes. Before
TASK-743 these lived inline in the application files behind raw
`#if defined(ESP8266) / #else` blocks, e.g. in `SATcycles.ino`:

```cpp
#if defined(ESP8266)
  #define SAT_FLOW_SAMPLE_SIZE 64
#else
  #define SAT_FLOW_SAMPLE_SIZE 256
#endif
```

and the matching ring-index counters duplicated as `uint8_t` (ESP8266) vs
`uint16_t` (ESP32). The TASK-739 audit counted each of these as an
abstraction-boundary violation (raw platform `#ifdef` in application code).

The literal wording of the Tier 3 task (TASK-743) said "move per-platform tuning
constants into `platform_*.h`". That turns out to be the wrong home:

- `platform.h` is only pulled in by `debugStuff.h` / `networkStuff.h`, **not**
  early in the main `OTGW-firmware.h` include chain. Moving SAT/MQTT sizing
  constants into `platform_*.h` would force `platform.h` (and with it
  `WiFi.h` / `WebServer.h` / `LittleFS.h`) to be included very early, ahead of
  `SATtypes.h` and the `.ino` files that consume the sizes — a large include
  ripple for no benefit.
- `boards.h` is **already** included at `OTGW-firmware.h:33`, ahead of every
  `.ino` file and of `SATtypes.h`, and is already on the abstraction allowlist
  (`evaluate.py::ESP_ABSTRACTION_ALLOWED_FILES`). It already holds the per-board
  `#if BOARD_NODOSHOP_*` blocks where these capacity tunings naturally belong.

## Decision

Per-board **compile-time numeric constants** and the **typedefs derived from
them** live in `boards.h`, inside the existing per-board
`#if defined(BOARD_NODOSHOP_*)` blocks — **not** in `platform_*.h` and **not**
inline behind raw `#if defined(ESP8266)` in application files.

This amends ADR-061's home assignment as follows:

| Category | Home |
|---|---|
| Divergent runtime **API** (heap, WiFi, NTP, LED, …) | `platform_*.h` shim |
| Board **pin maps** | `boards.h` |
| Board **capability flags** (`HAS_*`) | `boards.h` |
| Board **numeric tuning constants** (buffer/ring sizes, heap floors, timing) | `boards.h` *(this ADR)* |
| **Typedefs whose width depends on a board size** (e.g. ring-index counter) | `boards.h` *(this ADR)* |

The dividing line between `platform_*.h` and `boards.h`: `platform_*.h` answers
*"how does this platform do X"* (behaviour); `boards.h` answers *"how big / how
many / what width for this board"* (capacity). A value that is purely a number
or a typedef chosen by the board's RAM/flash budget is capacity, so it goes in
`boards.h`.

Concrete constants relocated under this decision (TASK-743 Tier 3a):
`SAT_WIN4H_SIZE`, `SAT_FLOW_SAMPLE_SIZE`, `SAT_TAIL_SAMPLE_SIZE`, `HCR_DAYS`,
`HCR_INTRADAY_SIZE`, `SAT_CYCLES_FILE_BUF_SIZE`, `MQTT_DISCOVERY_HEAP_MIN`,
`STATUS_BURST_COOLDOWN_MS`, plus the new `SAT_RING_IDX_T` typedef
(`uint8_t` on ESP8266, `uint16_t` on ESP32).

## Consequences

**Positive:**
- Removes 11 raw `#if defined(ESP8266/ESP32)` sites from application files
  (`SATcycles.ino`, `SATtypes.h`, `MQTTstuff.ino`); abstraction baseline
  33 → 22.
- One obvious home for "how big is this buffer on this board"; the two board
  blocks sit side by side, so a contributor sets both values in one place.
- No new include-order coupling: `boards.h` is already early and allowlisted.

**Negative:**
- `boards.h` grows beyond pure pin/flag declarations into sizing config. Reviewers
  must keep both board blocks in sync when adding a constant (a value added to one
  block but not the other is a compile error on the missing board, so the failure
  is loud, not silent).
- A constant that later needs *behaviour* (not just a number) must move to a
  shim; the categorisation table above is the test for which way a new value goes.

## Enforcement

Guideline-level under ADR-080, sharing the ESP-abstraction CI gate:
`evaluate.py::check_esp_abstraction_boundary()`. Moving a constant out of an
application file into `boards.h` removes its raw `#ifdef` from the scan, so the
baseline ratchet already rewards compliance and blocks regression. No separate
gate is added; the existing boundary scan is the mechanism.

## Related
- ADR-061: Unified ESP8266/ESP32 Platform Abstraction (this ADR amends its home-assignment for board-numeric constants)
- ADR-051: Settings/State architecture (Hungarian-prefixed config lives in structs; this ADR covers compile-time board constants, a distinct category)
- ADR-079: Per-component type headers (`<Component>types.h`); board-sized typedefs that several components share live in `boards.h` rather than any one component header
