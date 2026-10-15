---
id: "ADR-178"
title: "Widen the SAT area-sensor reference to accept a Bluetooth sensor address alongside a Dallas address"
status: "Accepted"
date: "2026-10-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
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
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-178 Widen the SAT area-sensor reference to accept a Bluetooth sensor address alongside a Dallas address

## Status

Accepted, 2026-10-15.

## Status History

```yaml
status_history:
  - date: 2026-09-22
    status: Proposed
    changed_by: "User: Robert van den Breemen"
    reason: Initial proposal
    changed_via: adr-kit
  - date: 2026-10-15
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Maintainer accepted 2026-10-15 after the code-vs-ADR verification; open question answered in place (TASK-1237)
    changed_via: adr-kit lifecycle
```

## Context

SAT can drive up to four heating areas, each fed by its own temperature sensor. The
mapping from area to sensor is persisted in `settings.sat.sSensorArea`, which was
declared `char[4][17]`: sixteen hex characters for a Dallas DS18B20 ROM (read-only memory) code plus a
terminator (`SATtypes.h:521` before this change; the widened field now sits at
`SATtypes.h:526`). The settings UI was branded accordingly ("DS18B20 Area Sensor
Mapping", in the classic UI's `data/index.js` at line 4131 at the time; historical
reference, that file was removed when the classic UI was retired, ADR-192) and its
dropdown was populated only from `/api/v2/sensors`.

That made the feature impractical for its main use. Reported on Discord
(#dev-sat-mqtt, 2026-06-21) by sergeantd: *"it is a bit difficult to hardwire a
ds18b20 [to another room], but it is pretty simple to add a ble sensor"*, seconded by
number3nl: *"ds18b20 is not a logical way to work here"*. Running a wire to a bedroom
is exactly the cost a multi-area feature should avoid.

The firmware already discovers and rosters BLE (Bluetooth Low Energy) sensors: `settings.sat.sBleMac` holds up
to `SAT_BLE_MAX_ROSTER` uppercase MACs in `AA:BB:CC:DD:EE:FF` form (`SATtypes.h:557`),
exposed over `/api/v2/sat/ble/discovery`. The data existed; only the mapping surface
refused it.

The blocking constraint was width. A MAC (media access control) address in that notation is 17 characters plus a
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

- `tests/webui/sat-area-sensor-mapping.test.mjs` — drives the shipped v2 UI (`v2.js`,
  `populateAreaSensorChoices()`) against both sensor sources and asserts a selected BLE
  MAC reaches `POST /api/v2/settings` (`satsensorarea0`) intact. Rebuilt for v2 in
  TASK-1234 after the classic UI and its test were removed (ADR-192).
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
  (`sBleMac`) and the MQTT (Message Queuing Telemetry Transport) topics, which both use the colon form. Saving one byte is
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
- 4 bytes of extra RAM (random-access memory) in the settings struct (4 areas x 1 byte).

**Negative:**

- The persisted schema changes. Old Dallas-only values load unchanged, but a settings
  file written by this firmware can contain a 17-character MAC that an older build does
  not reject: its loader `strlcpy`s the value into the old `[17]` field, truncating it
  to 16 characters, so the area silently matches no sensor at all. Downgrading is
  therefore not clean for an area mapped to a BLE sensor, and nothing tells the
  operator. *Mitigation:* the value is per-area and clearable from the UI; an operator
  who downgrades clears or remaps the affected areas.
- The shape contract is enforced on one write path only. `PATCH`
  `/api/v2/sat/sensor-areas` (the `sensor-areas` branch in `restAPI.ino`) validates the
  value, but the v2 UI saves through `POST /api/v2/settings`, which ends in
  `updateSetting()` (`settingStuff.ino`, the `SATsensorarea0..3` branches) and
  `strlcpy`s the value into the 18-byte field with no shape check. A malformed value is
  therefore stored as-is and matches no sensor. Impact is low: once any sensor is
  known the v2 UI offers a picker filled from the discovered Dallas and BLE lists
  (`populateAreaSensorChoices()` in `v2.js`; with no sensors it leaves a free-text
  field), and a bad value fails safe (the area gets no writer and drops out as stale).
- An area whose BLE sensor goes quiet holds its last value for up to about ten minutes
  before it is dropped, with no per-area fallback (see the answered Open Question
  below). While the roster slot is fresh (within `BLE_STALE_MS`, 300 s, `SATble.ino`)
  every pass of `satBLEUpdateState()` calls `satSetAreaTemp()` with the last sample,
  which re-stamps `state.sat.iAreaLastMs` with the current time, not the sample's age.
  After `BLE_STALE_MS` the fan-out stops, and the area stays valid for up to
  `SAT_AREA_STALE_MS` (300 s, `SATcontrol.ino`) more before
  `satGetWeightedRoomTemp()` marks it invalid and leaves it out of the average. When
  every area is stale the weighted average is `NAN` and the room-temperature source
  falls through to the external temperature, then OpenTherm Tr (MsgID 24), then the
  existing skip and safety-trip chain. *Possible follow-up (not decided):* stamp the
  area with the sample's own `iLastSeenMs` instead of `millis()`, which would shorten
  the hold to about five minutes.
- Two sources now feed `satSetAreaTemp()`, so a future reader must know that the
  reference shape decides ownership. *Mitigation:* stated in the field's own comment in
  `SATtypes.h` and in the Decision Contract above.
- The BLE fan-out runs inside `satBLEUpdateState()`, so an area fed by BLE updates on
  the BLE cadence rather than the Dallas poll cadence. *Mitigation:* both are well
  inside SAT's control interval; stale BLE slots are already excluded by
  `BLE_STALE_MS`.

## Open Questions

- [x] Should an area whose mapped BLE sensor goes stale fall back to another sensor, or
      hold its last value? The BLE room-temperature path has failover
      (`settings.sat.bBleFailover`, TASK-762); area mapping deliberately does not, since
      a fallback sensor in a different room would report the wrong area. Confirm that
      holding is the wanted behaviour before this ADR is accepted.
  **Answered 2026-10-15 by User: Robert van den Breemen:** no per-area fallback; the
  area holds its last value for a bounded time and is then dropped, which is what the
  code does today. While the roster slot is fresh, each `satBLEUpdateState()` pass
  calls `satSetAreaTemp()` with the last sample and re-stamps `iAreaLastMs`. After
  `BLE_STALE_MS` (300 s) the fan-out stops; the area stays valid up to
  `SAT_AREA_STALE_MS` (300 s more), then `satGetWeightedRoomTemp()` drops it. The hold
  is therefore at most about ten minutes. If every area is stale the average is `NAN` and
  control falls through to the external temperature, then Tr, then the safety-trip
  chain. Recorded under Consequences; shortening the hold to about five minutes by
  stamping the sample's own `iLastSeenMs` is a possible follow-up, not decided.

## Related Decisions

- **ADR-051 (Settings and State architecture)**: this widens a field inside
  `OTGWSettings`, keeping the Hungarian prefix and two-level section layout.
- **ADR-165 (REST/web concurrency cap)**: the UI gained a fourth `GET` for the BLE
  roster; all mapping fetches stay sequential so the N<=2 in-flight cap holds.

## References

- TASK-1153 (backlog), reported on Discord #dev-sat-mqtt 2026-06-21 by sergeantd,
  seconded by number3nl.
- `src/OTGW-firmware/SATtypes.h` — `sSensorArea` declaration.
- `src/OTGW-firmware/restAPI.ino` — `sensor-areas` GET/PATCH handler and the
  `satsensorarea*` settings entries.
- `src/OTGW-firmware/SATble.ino` — area fan-out in `satBLEUpdateState()`.
- `src/OTGW-firmware/sensors_ext.ino:298` — the Dallas side of the same fan-out.
- `tests/webui/sat-area-sensor-mapping.test.mjs` — behavioural guard (v2 UI, TASK-1234).

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
