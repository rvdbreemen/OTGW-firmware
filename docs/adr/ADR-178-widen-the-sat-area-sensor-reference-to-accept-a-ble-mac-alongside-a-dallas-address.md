---
id: "ADR-178"
title: "Widen the SAT area-sensor reference to accept a BLE MAC alongside a Dallas address"
status: "Proposed"
date: "2026-09-22"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "sat area sensors"
  - "settings schema"
  - "ble sensors"
  - "dallas sensors"
aliases:
  - "area sensor mapping"
  - "satsensorarea"
  - "sSensorArea"
components:
  - "settings.sat.sSensorArea"
  - "/api/v2/sat/sensor-areas"
symbols:
  - "sSensorArea"
  - "satSetAreaTemp"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-178 Widen the SAT area-sensor reference to accept a BLE MAC alongside a Dallas address

## Status

Proposed, 2026-09-22.

## Status History

```yaml
status_history:
  - date: 2026-09-22
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
```

## Context

SAT can drive up to four heating areas, each fed by its own temperature sensor. The
mapping from area to sensor is persisted in `settings.sat.sSensorArea`, which was
declared `char[4][17]`: sixteen hex characters for a Dallas DS18B20 ROM code plus a
terminator (`SATtypes.h:521`, before this change). The settings UI was branded
accordingly ("DS18B20 Area Sensor Mapping", `data/index.js:4131`) and its dropdown was
populated only from `/api/v2/sensors`.

That made the feature impractical for its main use. Reported on Discord
(#dev-sat-mqtt, 2026-06-21) by sergeantd: *"it is a bit difficult to hardwire a
ds18b20 [to another room], but it is pretty simple to add a ble sensor"*, seconded by
number3nl: *"ds18b20 is not a logical way to work here"*. Running a wire to a bedroom
is exactly the cost a multi-area feature should avoid.

The firmware already discovers and rosters BLE sensors: `settings.sat.sBleMac` holds up
to `SAT_BLE_MAX_ROSTER` uppercase MACs in `AA:BB:CC:DD:EE:FF` form (`SATtypes.h:552`),
exposed over `/api/v2/sat/ble/discovery`. The data existed; only the mapping surface
refused it.

The blocking constraint was width. A MAC in that notation is 17 characters plus a
terminator, so a `[17]` field cannot hold one: the value would land truncated or be
rejected. Widening a persisted settings field changes the on-disk schema and the REST
contract, which is why this is a decision rather than an implementation detail.

## Decision

`settings.sat.sSensorArea` becomes `char[4][18]` and holds a *sensor reference* that is
either a 16-character Dallas address or a 17-character BLE MAC; the value's own length
identifies which source owns that area.

Length is the discriminator because the two forms cannot collide: a Dallas ROM code is
16 hex characters with no separators, a MAC is 17 characters with colons at fixed
positions. No type tag, no second field, no parallel array.

Each area therefore keeps exactly one writer. The Dallas poll loop
(`sensors_ext.ino:298`) claims the areas whose reference matches a discovered ROM code;
the BLE update path (`SATble.ino`, inside `satBLEUpdateState()`'s fresh-slot loop)
claims the areas whose reference matches a rostered MAC. Since an area holds one
reference, the two paths cannot both write the same area.

## Decision Contract

### Must

- `settings.sat.sSensorArea[n]` is sized for the wider form (at least 18 bytes).
- `PATCH /api/v2/sat/sensor-areas` accepts an empty string (clear), 16 hex characters,
  or a 17-character MAC with colons at positions 2, 5, 8, 11 and 14, and normalises to
  uppercase. Anything else is a 400.
- A settings file written before this change, holding Dallas-only values, loads
  unchanged.
- Each area has one in-firmware writer. A source claims an area only when the stored
  reference matches that source's own address form.

### Must Not

- Do not add a separate type/source field to disambiguate. The value's shape is the
  discriminator, and a second field could disagree with it.
- Do not widen the field without widening the `satsensorarea*` settings maxima
  (`restAPI.ino`), or a valid MAC is silently truncated on the settings round-trip.

### Exceptions

- None.

### Verification

- `tests/webui/sat-area-sensor-mapping.test.mjs` — drives the shipped UI against both
  sensor sources and asserts a selected BLE MAC reaches
  `PATCH /api/v2/sat/sensor-areas` intact.
- `src/OTGW-firmware/SATtypes.h` — field declaration and its width.
- `src/OTGW-firmware/restAPI.ino` — the `sensor-areas` validation branch.

## Alternatives Considered

- **Keep Dallas-only and add a separate BLE-area mapping.** Rejected: two mapping
  tables for one concept. The UI would need two pickers, the firmware two lookup paths,
  and nothing would stop both from claiming the same area, which is precisely the
  single-writer property this decision preserves for free.
- **Store a compact MAC (12 hex, no colons) so the existing `[17]` field fits.**
  Rejected: it collides in spirit with the Dallas form (both become bare hex strings),
  so length alone would no longer identify the source, and it diverges from the roster
  (`sBleMac`) and the MQTT topics, which both use the colon form. Saving one byte is
  not worth introducing a second MAC notation.
- **Add an explicit `source` field per area.** Rejected: a tag that can disagree with
  the value it describes is a state you then have to reconcile. The value already
  carries the answer.
- **Do nothing.** Rejected: it leaves the reported friction in place. A multi-area
  feature whose only sensor option must be wired to each room is a feature most users
  cannot complete.

## Consequences

**Positive:**

- A BLE sensor can be assigned to a SAT area, which is the practical way to cover rooms
  away from the gateway.
- The settings UI drops the DS18B20 branding and lists both sources in one picker, each
  marked with its origin, so the two 16-and-17-character strings cannot be confused.
- Single-writer-per-area is preserved structurally rather than by convention.
- 4 bytes of extra RAM in the settings struct (4 areas x 1 byte).

**Negative:**

- The persisted schema changes. Old Dallas-only values load unchanged, but a settings
  file written by this firmware can contain a value that an older build would reject as
  over-long. Downgrading is therefore not clean for an area mapped to a BLE sensor.
  *Mitigation:* the value is per-area and clearable from the UI; an operator who
  downgrades clears the affected areas.
- Two sources now feed `satSetAreaTemp()`, so a future reader must know that the
  reference shape decides ownership. *Mitigation:* stated in the field's own comment in
  `SATtypes.h` and in the Decision Contract above.
- The BLE fan-out runs inside `satBLEUpdateState()`, so an area fed by BLE updates on
  the BLE cadence rather than the Dallas poll cadence. *Mitigation:* both are well
  inside SAT's control interval; stale BLE slots are already excluded by
  `BLE_STALE_MS`.

## Open Questions

- [ ] Should an area whose mapped BLE sensor goes stale fall back to another sensor, or
      hold its last value? The BLE room-temperature path has failover
      (`settings.sat.bBleFailover`, TASK-762); area mapping deliberately does not, since
      a fallback sensor in a different room would report the wrong area. Confirm that
      holding is the wanted behaviour before this ADR is accepted.

## Related Decisions

- **ADR-051 (Settings and State architecture)**: this widens a field inside
  `OTGWSettings`, keeping the Hungarian prefix and two-level section layout.
- **ADR-165 (REST/web concurrency cap)**: the UI gained a fourth GET for the BLE
  roster; all mapping fetches stay sequential so the N<=2 in-flight cap holds.

## References

- TASK-1153 (backlog), reported on Discord #dev-sat-mqtt 2026-06-21 by sergeantd,
  seconded by number3nl.
- `src/OTGW-firmware/SATtypes.h` — `sSensorArea` declaration.
- `src/OTGW-firmware/restAPI.ino` — `sensor-areas` GET/PATCH handler and the
  `satsensorarea*` settings entries.
- `src/OTGW-firmware/SATble.ino` — area fan-out in `satBLEUpdateState()`.
- `src/OTGW-firmware/sensors_ext.ino:298` — the Dallas side of the same fan-out.
- `tests/webui/sat-area-sensor-mapping.test.mjs` — behavioural guard.

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "sSensorArea\\s*\\[\\s*4\\s*\\]\\s*\\[\\s*(?:1[0-7]|[0-9])\\s*\\]",
      "path_glob": "src/OTGW-firmware/**/*.h",
      "message": "sSensorArea must stay at least [18] wide: a BLE MAC is 17 chars plus terminator (ADR-178)."
    }
  ],
  "forbid_import": [],
  "require_pattern": []
}
```
