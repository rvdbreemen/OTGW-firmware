---
id: "ADR-291"
title: "The Monitor graph history stores the room SAT regulates on in its own column; a slot grows from 16 to 18 bytes (amends ADR-256)"
status: "Accepted"
date: "2026-11-21"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "graph-history"
  - "sat"
  - "web-ui"
aliases:
  - "satRoom column"
  - "SAT room in the graph history"
  - "TASK-3007"
components:
  - "GraphHistSlot satRoom column"
symbols:
  - "GraphHistSlot"
  - "satRoom"
  - "graphHistEmitJson"
  - "GRAPH_HIST_NO_TEMP"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-291 The Monitor graph history stores the room SAT regulates on in its own column; a slot grows from 16 to 18 bytes (amends ADR-256)

## Status

Accepted, 2026-11-21. Guideline-level per ADR-080: verified by host and web UI tests, no evaluate.py gate. Amends ADR-256 (the slot size), and through it ADR-193 and ADR-245.

**Decision Maker:** the coordinator chose option (b) of the architect's comparison for TASK-3007 on 2026-11-21. Robert accepts.

## Status History

```yaml
status_history:
  - date: 2026-11-21
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-11-21
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer ("Accepteren", interactive answer in the otgw-poc-dev session, 2026-11-21)
    changed_via: adr-kit
```

## Context

The device's Monitor graph history (ADR-193, ADR-245) stores a slot every 30 s, or every 120 s on a board with PSRAM. Since ADR-256, a slot is 16 bytes: seven `int16_t` temperatures (`flow`, `ret`, `chSet`, `roomSet`, `room`, `outside`, `satTarget`), `mod` and `flags` (`src/OTGW-firmware/graphHistoryStuff.h:63-74`, `static_assert` at `:74`).

**The `room` column always holds the thermostat's room temperature, MsgID 24** (`graphHistoryStuff.ino:65-67`). While SAT is enabled, SAT may regulate on another room temperature. Its source order is BLE, multi-area, external (MQTT) and only then MsgID 24 (`satGetRoomTemp()`, `SATcontrol.ino:2759`).

**TASK-3007 is the case that shows the gap.** will_85903's capture (`OTGW-field-data/will_85903/2026-11-21_upload/`, beta.392, `satexternaltemp=true` with an MQTT room sensor):

- the graph showed the thermostat room at 20.4 C and up;
- SAT's own room, in the SATTICK lines, read about 1 C lower (19.3-20.1 C);
- the capture therefore looked like overheating that was not there.

**Three options were compared** (architect, 2026-11-21):

- (a) SAT's room in the existing `room` field while SAT is enabled, with a flag bit for the source;
- (b) a column of its own;
- (c) only the export or UI showing SAT's room, taken from MQTT history.

## Decision

The device history slot gets an eighth `int16_t` temperature column, `satRoom`, so a slot is 18 bytes. The column holds the room temperature SAT regulates on while SAT is enabled, and "no value" otherwise. The graph and its CSV export show it as its own series. This follows ADR-256's pattern for `satTarget` decision by decision.

1. **Slot.**
   - `GraphHistSlot` gains `int16_t satRoom` after `satTarget`, in tenths of a degree, with `GRAPH_HIST_NO_TEMP` (`INT16_MIN`) for "no value".
   - The `static_assert` becomes 18 bytes.
   - The ring capacities, intervals, the 6-slot writer margin and the PSRAM block of ADR-193 and ADR-245 stay as they are; only the byte count per slot changes.
2. **Source.**
   - **The value.** The sampler writes the room temperature SAT uses for its control, the value `satGetRoomTemp()` returns. That function is `static` in `SATcontrol.ino`, so the implementation exposes it through a getter or a state field; `state.sat.fRoomTemp` (`SATtypes.h:132`) is the candidate, and otgw-dev confirms which one holds the same value.
   - **When it is "no value".** It writes `GRAPH_HIST_NO_TEMP` when SAT is not enabled, and when SAT's room is lost (`state.sat.bRoomTempLost`, the same rule the `room` column uses, `graphHistoryStuff.ino:66`).
3. **Response.**
   - **When the array appears.** GET `/api/v2/graph/history` emits a `satRoom` array only when at least one slot of the served window has a value, exactly as `satTarget` (`graphHistoryStuff.h:305`).
   - **Users without SAT see no change.** A window in which SAT was never enabled gets the body of ADR-256 byte for byte.
   - **Grouping.** With `?hours=` grouping (ADR-245 decision 3), a group takes the newest value that is not empty.
4. **Browser.**
   - `data/v2.js` reads `satRoom` when present (absent means empty) and draws it as its own series, with its own legend entry and colour. The CSV export carries a `satRoom` column.
   - The live 5 s records take SAT's room from the SAT status the page already polls, so there is no extra request (at most 2 API calls in flight).
   - The thermostat room series (`room`, MsgID 24) stays unchanged.

## Decision Contract

### Must

- Keep `GraphHistSlot` at exactly 18 bytes, with `satRoom` as the eighth `int16_t` temperature.
- Write `GRAPH_HIST_NO_TEMP` in `satRoom` while SAT is not enabled, and while SAT's room is lost.
- Omit the `satRoom` array from a response whose window holds no SAT room value, so that body is unchanged.
- Keep the `room` column as MsgID 24 only.

### Must Not

- Write SAT's room into `room` or any other existing column (option (a) rejected).
- Change the ring capacities, intervals or the writer margin of ADR-193 and ADR-245.
- Add a REST call for SAT's room to the graph's live path.

### Exceptions

- None.

### Verification

- **`tests/test_graph_history.py`** compiles `tests/test_graph_history.cpp` on the real `graphHistoryStuff.h`. It checks:
  - the slot is 18 bytes;
  - a window with SAT room values emits `satRoom` with the right values and grouping;
  - a lost SAT room stores "no value";
  - a window without values emits a body byte-identical to the code before this ADR.
- **A web UI test** in `tests/webui/` drives the real `data/v2.js`:
  - the SAT room series and legend appear with a device history that has `satRoom`;
  - the CSV export has the column;
  - a history without it draws no series.
- **Field:** a capture from will_85903 (or another tester with `satexternaltemp=true`) shows `satRoom` matching SAT's room in the SATTICK lines.

## Alternatives Considered

- **(a) SAT's room in the existing `room` field, with a flag bit for the source.** It costs no RAM, since 5 bits of `flags` are free. Rejected:
  - the `room` key would change its meaning per slot, so every reader (graph, CSV, capture analysis, replay) would have to read the bit, or a mode switch looks like a jump;
  - the thermostat's MsgID 24 room would vanish from the history while SAT is enabled, and the comparison of the two rooms is exactly what explains will_85903's capture.
- **(c) The export or UI shows SAT's room from MQTT history.** Rejected: the device keeps no MQTT history. It would depend on Home Assistant or the broker, so the on-device graph and the capture-tool download stay wrong, against local-first diagnostics. It needs no ADR, but it does not solve the problem.
- **Do nothing.** Rejected: captures with an external SAT room sensor keep showing overheating that is not there.

## Consequences

**Positive:**

- **Both rooms are visible side by side**, so a gap between the thermostat and SAT's sensor shows in the graph itself.
- **Additive.** Old readers ignore the new JSON key; a history without SAT is unchanged byte for byte.

**Negative:**

- **RAM, +2 bytes per slot:**
  - on a board with PSRAM, about 10.1 KB more PSRAM (5,046 slots x 2 B);
  - on a board without PSRAM, 252 B more static internal RAM (126 slots x 2 B).

  Mitigation: the PSRAM block is allocated once; 252 B of `.bss` is part of the baseline, not a heap dip.
- **Flash:** a few hundred bytes at most (one emit line and one encoder). That counts against the TASK-1993 budget (TASK-2980).

## Related Decisions

- **ADR-256** (`satTarget` column, slot 14 -> 16 B, Accepted): amended. The slot becomes 18 B; its pattern is followed decision by decision.
- **ADR-193 / ADR-245** (device history, 24 h and 7 d rings): the ring sizes and intervals are unchanged; only the bytes per slot change.
- **ADR-248** (the HA climate entity follows SAT): the same principle, SAT's own values shown as SAT's.

## References

- TASK-3007, with the capture `OTGW-field-data/will_85903/2026-11-21_upload/`.
- `src/OTGW-firmware/graphHistoryStuff.h:63-74` (slot), `:298-317` (JSON columns, `satTarget` optional at `:305`).
- `src/OTGW-firmware/graphHistoryStuff.ino:55-75` (sampler; `room` at `:65-67`).
- `src/OTGW-firmware/SATcontrol.ino:2759` (`satGetRoomTemp()`, static).
- `src/OTGW-firmware/SATtypes.h:132` (`fRoomTemp`, candidate source).
- Readers: `src/OTGW-firmware/data/v2.js`, `scripts/otgw_capture.py`.
