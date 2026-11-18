---
id: "ADR-256"
title: "The Monitor graph history stores the SAT target in its own column; a slot grows from 14 to 16 bytes (amends ADR-193 and ADR-245)"
status: "Proposed"
date: "2026-11-18"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
related:
  - "ADR-193"
  - "ADR-245"
topics:
  - "graph"
  - "sat"
  - "rest-api"
  - "web-ui"
  - "csv-export"
aliases:
  - "SAT target in the graph"
  - "satTarget column"
  - "graph history slot 16 bytes"
components:
  - "Monitor graph device history slot layout (graphHistoryStuff)"
  - "GET /api/v2/graph/history satTarget column"
symbols:
  - "GraphHistSlot"
  - "graphHistEmitTempColumn"
  - "GRAPH_HIST_NO_TEMP"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-256 The Monitor graph history stores the SAT target in its own column; a slot grows from 14 to 16 bytes (amends ADR-193 and ADR-245)

## Status

Proposed, 2026-11-18.

**Decision Maker:** User: Robert van den Breemen (maintainer). Discord direct message 1572585729098383494,
2026-11-18 12:14:15 UTC, reply "A" to question 1572584352477618247 (see Decision).

## Status History

```yaml
status_history:
  - date: 2026-11-18
    status: Proposed
    changed_by: Agent (ba)
    reason: "Written by otgw-webui (Claude) on Robert's choice A, Discord DM 1572585729098383494 on question 1572584352477618247 (TASK-2018); acceptance is his"
    changed_via: adr-kit
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-193
    changed_via: adr-kit lifecycle
  - date: 2026-11-18
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Related to ADR-245
    changed_via: adr-kit lifecycle
```

## Context

The Monitor graph of the v2 web UI (user interface) and its CSV (comma-separated values) export draw the thermostat
setpoint, OpenTherm message ID (MsgID) 16 `TrSet`, as the room setpoint series: in the browser from the live
OpenTherm frames (`data/v2.js:2392`), and from the device history ring (`graphHistoryStuff.ino:61`). While SAT (Smart
Autotune Thermostat, the firmware's own room controller) is enabled, the room temperature the boiler is steered to is
the SAT target (`settings.sat.fTargetTemp`, the `target_temp` of `/api/v2/sat/status`, `SATcontrol.ino:4794`), not the
thermostat setpoint. The graph never shows it. BlueFlame's analysis of a tester's graph CSV files (TASK-1998,
2026-11-18) found the gap: the CSV said "thermostat setpoint" while SAT was steering to another value.

ADR-193 fixed the device history slot at 14 bytes: six `int16_t` temperatures in tenths of a degree (flow, return,
control setpoint, thermostat setpoint, room, outside), modulation and a flags byte (`graphHistoryStuff.h:62-72`, a
`static_assert` holds the size). ADR-245 sized the PSRAM (pseudo-static random-access memory) ring at 5046 slots,
7 days of 2 minutes, and kept the default response body of ADR-193. There is no field in the slot where a SAT target
could go without changing the meaning of an existing column.

The ring sizes, measured from the code on origin/dev 0e967cb71:

| Board | Ring | 14 B slots | 16 B slots | Difference |
|---|---|---|---|---|
| Without PSRAM (static array, internal RAM) | 126 slots of 30 s, 1 h | 1764 B | 2016 B | +252 B |
| With PSRAM (`platformPsramAllocOnce`) | 5046 slots of 120 s, 7 d | 70644 B | 80736 B | +10092 B |

The response is the costly part. ADR-245 records that `restSendChunked()` re-runs the whole emitter for every TCP
(Transmission Control Protocol) send window, so the work per response grows with the square of the body size, on the
async_tcp task that serves every other web request. A seventh temperature column of 720 slots (24 h at 2 minutes)
adds about 2.9 to 3.6 KB to a body of about 25 KB: about 12 to 14 % more body and, by the square rule, about 25 to 30 %
more formatting work. This is an estimate from the value widths ("205," is 4 characters, "null," 5); the
implementation measures it (Verification).

Robert was asked (question 1572584352477618247, item 2 of 3) and chose A. The option text, verbatim: "A) Eigen kolom in
de grafiekhistorie (slot 14 -> 16 B): +252 B intern RAM zonder PSRAM, +10 KB PSRAM, eerlijke lijn + CSV-kolom; vraagt
een ADR die 193/245 aanvult." His reply, verbatim: "A".

## Decision

**User Decision:** the device history slot gets a seventh `int16_t` temperature column, `satTarget`, so a slot is
16 bytes; the column holds the SAT target while SAT is enabled and "no value" otherwise, and the graph and its CSV export
show it as its own series.

1. **Slot.** `GraphHistSlot` gains `int16_t satTarget` after `outside`, in tenths of a degree with `GRAPH_HIST_NO_TEMP`
   (`INT16_MIN`) for "no value"; the `static_assert` becomes 16 bytes. The ring sizes, intervals, the 6-slot writer
   margin and the PSRAM block of ADR-193 and ADR-245 stay as they are; only the byte count per slot changes.
2. **Source.** The sampler writes `settings.sat.fTargetTemp` when `satIsEnabled()`, the value the SAT room tile and
   `target_temp` show; otherwise `GRAPH_HIST_NO_TEMP`. The PV (photovoltaic) surplus boost that `satEffectiveTarget()`
   adds is not included: the column is the target the user set, comparable with the thermostat setpoint.
3. **Response.** GET `/api/v2/graph/history` emits a `satTarget` array next to the other temperature columns only when
   at least one slot of the served window has a value. A window in which SAT was never enabled gets the body of
   ADR-193 and ADR-245 byte for byte, so users without SAT pay nothing. The check scans the snapshot's slots, so every
   re-run of the emitter for a send window decides the same way (the byte-identical rule of ADR-193). With
   `?hours=` grouping (ADR-245 decision 3) a group takes the newest value that is not empty, as the other
   temperatures do.
4. **Browser.** `data/v2.js` reads `satTarget` when present (absent means empty), draws it as its own series with its
   own legend entry and colour, and the CSV export carries a `satTarget` column. The live 5 s records take the
   target from the SAT status the page already polls; no extra request (at most 2 API calls in flight). The
   thermostat setpoint series and its MsgID 16 label stay unchanged.

## Decision Contract

### Must

- Keep `GraphHistSlot` at exactly 16 bytes, with `satTarget` as the seventh `int16_t` temperature.
- Write `GRAPH_HIST_NO_TEMP` in `satTarget` while SAT is not enabled.
- Omit the `satTarget` array from a response whose window holds no SAT target value, so that body is unchanged.
- Keep the thermostat setpoint column (`roomSet`) as MsgID 16 only.

### Must Not

- Write the SAT target into `roomSet` or any other existing column.
- Change the ring capacities, intervals or the writer margin of ADR-193 and ADR-245.
- Add a REST call for the SAT target to the graph's live path.

### Exceptions

- None.

### Verification

- `tests/test_graph_history.py` (compiles `tests/test_graph_history.cpp` on the real `graphHistoryStuff.h`): the slot is
  16 bytes; a window with SAT values emits `satTarget` with the right values and grouping; a window without them emits
  a body byte-identical to the code before this ADR; the body size of a dense 24 h window with and without the column
  is printed and recorded in TASK-2018.
- A web UI test in `tests/webui/` drives the real `data/v2.js`: the SAT target series and legend appear with a device
  history that has `satTarget`, the CSV export has the column, and a history without it draws no series. It fails on
  the code before this ADR.

## Alternatives Considered

- **B: live only, from `/api/v2/sat/status` in the browser.** No ring change, but no history: after a page reload the
  series starts empty and the CSV export of an earlier day has no SAT target. Rejected by Robert's choice of A.
- **C: write the SAT target into the thermostat setpoint column while SAT is on.** No format change, but the column
  and its "thermostat setpoint" legend and CSV header would carry a different quantity depending on the mode, with
  nothing in the slot to tell which (the flags byte holds only flame, DHW and CH); a window that spans a SAT on/off
  switch mixes the two, the confusion that started this task. The thermostat's own setpoint would also be lost while
  SAT runs. Rejected.
- **Do nothing.** The graph keeps showing a setpoint that does not steer the boiler while SAT runs. Rejected.

## Consequences

**Positive:**

- The graph and the CSV show the room target that SAT regulates to next to the thermostat setpoint, for the whole
  history the ring holds (1 h without PSRAM, 7 days with it).
- Each column keeps one meaning, so CSV analyses need no knowledge of the SAT mode.
- A response for a window without SAT is unchanged in bytes and cost.

**Negative:**

- +252 B of internal RAM (`.bss`) on boards without PSRAM and +10092 B of PSRAM on boards with it.
- While SAT is enabled, a full 24 h response is an estimated 12 to 14 % larger and about 25 to 30 % more formatting
  work on async_tcp (to be measured). Mitigation: the column is omitted when the window has no SAT value; TASK-1306
  tracks the response cost.
- The ring is RAM only, so there is no stored format to migrate; an older page that does not know `satTarget` ignores
  the extra array. A newer page against older firmware sees no `satTarget` and draws no series.
- Test fixtures that build ring JSON or assert the slot size change with it.

## Open Questions

- None.

## Related Decisions

- **ADR-193 (Keep a bounded 24 h Monitor graph history on the device):** amended: the slot layout grows from 14 to
  16 bytes; everything else stands.
- **ADR-245 (7 days on PSRAM boards, thinned responses):** amended: the PSRAM block grows by 10092 B, and the body is
  byte-identical to ADR-193 only for a window without SAT values.
- **ADR-146 (No ArduinoJson):** the column is written with the same JsonEmit path.

## References

- TASK-2018 (backlog), groundwork notes with the byte counts and the option comparison.
- `src/OTGW-firmware/graphHistoryStuff.h:41-47` (ring sizes), `:62-72` (slot and `static_assert`), `:244-257`
  (temperature column emitter), `:290-295` (column order).
- `src/OTGW-firmware/graphHistoryStuff.ino:21,29` (ring allocation), `:57-71` (sampler).
- `src/OTGW-firmware/SATcontrol.ino:4794` (`target_temp` = `settings.sat.fTargetTemp`), `:7664-7670`
  (`satEffectiveTarget()`).
- `src/OTGW-firmware/data/v2.js:2392` (MsgID 16 as `roomSet`), `:3212` (series list), `:3335` (device history columns),
  `:5884-5888` (`exportCsv()`).
- Discord direct messages 1572584352477618247 (question) and 1572585729098383494 (Robert's reply "A").
