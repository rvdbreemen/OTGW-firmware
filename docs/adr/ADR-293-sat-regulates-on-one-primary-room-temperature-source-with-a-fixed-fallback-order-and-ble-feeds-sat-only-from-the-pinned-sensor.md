---
id: "ADR-293"
title: "SAT regulates on one primary room temperature source with a fixed fallback order, and BLE feeds SAT only from the pinned sensor"
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
  - "sat"
  - "room temperature source"
  - "ble"
  - "mqtt"
aliases:
  - "SATroomsource"
  - "room_source"
  - "room_temp_source"
  - "BLE pin only"
components:
  - "satGetRoomTemp"
  - "satBLEUpdateState"
  - "SATroomsource"
symbols:
  - "satTryRoomSource"
  - "satRoomSourceParse"
  - "_sat_roomSrc"
  - "bBlePinNotice"
  - "SAT_ROOMSRC_AUTO"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-293 SAT regulates on one primary room temperature source with a fixed fallback order, and BLE feeds SAT only from the pinned sensor

## Status

Proposed, 2026-11-21.

## Status History

```yaml
status_history:
  - date: 2026-11-21
    status: Proposed
    changed_by: "Agent: Claude (otgw-dev)"
    reason: TASK-3011 / TASK-3015 initial proposal
    changed_via: adr-kit
  - date: 2026-11-21
    status: Accepted
    changed_by: "User: Robert van den Breemen"
    reason: Accepted by the maintainer in the coordinator session (AskUserQuestion, 2026-11-21) after the TASK-3011 bench passed
    changed_via: adr-kit
```

## Context

SAT needs one room temperature to regulate on. `satGetRoomTemp()` (src/OTGW-firmware/SATcontrol.ino) took the first valid value in a
fixed order: BLE, multi-area average, MQTT/REST external sensor, thermostat (MsgID 24). Two problems were reported by a tester
(gruijter, TASK-3011) and confirmed in the code on origin/dev:

1. **An unchosen BLE sensor could drive SAT.** BLE scans by default on boards with PSRAM (SATtypes.h `bBleEnable = true`). With no
   pinned MAC, `satBLEUpdateState()` (SATble.ino) drove SAT from the first fresh roster sensor, and any nearby advertiser enters the
   roster, a neighbour's sensor included. The TASK-508 auto-select pinned the only fresh sensor for good. A stale pin with
   `bBleFailover` (default on) switched to any other fresh roster sensor.
2. **The order was hidden and fixed.** BLE outranked an explicitly configured external sensor, and MQTT clients could not see which
   source SAT used (TASK-3015).

Robert decided the behaviour in entries 451, 453, 454, 455, 457 and 458 of the SAT decision register.

## Decision

SAT regulates on one primary room temperature source chosen by the setting `SATroomsource`, with a fixed fallback order, and BLE
feeds SAT only from the sensor the user pinned.

1. **Primary source.** `SATroomsource` is `auto` (0, default), `thermostat` (1), `external` (2), `ble` (3) or `areas` (4).
   `auto` is the fixed order pinned BLE > areas > external > thermostat. Any other value puts that source first; when it has no
   fresh value, the remaining sources follow in the auto order, then the thermostat, then none (heating curve only, the existing
   room-lost path of TASK-1364). A primary thermostat with a fresh MsgID 24 value is used before the other sources.
2. **BLE only from the pin.** Without a pinned MAC no BLE sensor drives SAT (it is still shown and published). The auto-select pin is
   removed. A stale pin with `bBleFailover` switches only to a fresh sensor the user assigned to an area (`sSensorArea`), never to an
   unchosen roster sensor. `bBleFailover` defaults to off.
3. **Migration, once** (marker `SATblepinonly`): `bBleFailover` is reset to off; every stored pin is kept as the user's choice and
   flagged (`bBlePinNotice`) until the user writes `SATblemac` (the same MAC confirms it); the SAT log names it once per boot and
   `/api/v2/sat/status` carries `ble_pin_notice`. `SATroomsource` starts at `auto` on every existing install.
4. **Interfaces.** Settings and REST key `satroomsource` (0..4), REST SAT setting `room_source`, MQTT command
   `set/<node>/sat/room_source` (a name or a number; anything else keeps the value), retained state `sat/room_source` (the name).
   The source SAT actually uses is published retained on `sat/room_temp_source` (ble, areas, external, thermostat, simulation,
   estimate, none), as the status field `room_temp_source`, and as one Home Assistant diagnostic sensor (flat topic, ADR-101).
5. **Recorded source.** `satGetRoomTemp()` records the source of every value it returns (`_sat_roomSrc`); the REST room age
   (`room_temp_age_s`, `satRoomTempSampleMs()`) reads that recorded source instead of repeating the order.

## Decision Contract

### Must

- `SATroomsource` default is `auto`, and `auto` gives today's order with BLE only when pinned.
- A primary without a fresh value falls back along the auto order, then the thermostat, then heating curve only.
- BLE drives SAT only from the pinned MAC, or with `bBleFailover` from a sensor assigned to an area.
- Every return of `satGetRoomTemp()` records its source; `sat/room_temp_source` and `room_temp_age_s` use that record.

### Must Not

- Pin a BLE sensor without a user action (no auto-select).
- Fail over to a roster sensor the user did not assign.
- Clear `bBlePinNotice` while `settings.ini` is being loaded.

### Exceptions

- None.

### Verification

- `tests/test_sat_ble_pin_only.py`: compiles the real `satBLEUpdateState()`, the `[sat-blepinonly-migrate]` block and
  `satGetRoomTemp()` with its helpers; BLE cases, migration cases and 7 primary-source cases. Red on origin/dev (10 failures) and on
  the code before the primary-source setting (3 failures), green on the change.
- `tests/test_sat_status_live.py` (room age follows the recorded source), `tests/test_sat_energy_discovery.py` (sensor table and index).

## Alternatives Considered

- **An ordered list of sources over MQTT** (the tester's proposal). Rejected by Robert (entry 454): more settings, UI and validation
  for a need nobody has shown; the MQTT command and retained state are kept as the interface for the one value.
- **Default `thermostat` for new installs.** Rejected (entry 457): a user who later enables the external sensor or areas would keep
  regulating on the thermostat without noticing; `auto` keeps today's behaviour for every setup except the BLE neighbour case.
- **Clear every stored BLE pin on upgrade.** Rejected (entry 455): the firmware cannot tell an auto-selected pin from a user's
  choice, and clearing would silently move correct setups to the thermostat; the pin is kept and the user asked to confirm it.

## Consequences

**Positive:**

- A neighbour's or any unchosen BLE sensor can no longer drive SAT.
- The user can make the external sensor, areas or the thermostat win over BLE, and MQTT/Home Assistant show the source in use.
- The REST room age stays right for every primary.

**Negative:**

- An install that relied on an unpinned BLE sensor now regulates on the next source until the user pins it; the SAT page and log
  ask for it. Mitigation: the one-time notice; existing pins keep working.
- A fifth value (`auto`) to explain in the UI. Mitigation: help text in settingsHelp.h; the web UI shows the active source.
- `bBleFailover` changes default and is reset once; users who wanted roster failover must turn it on and assign area sensors.

## Open Questions

- [ ] Web UI: setting control, the confirm banner for `ble_pin_notice` and the `room_temp_source` display (otgw-webui, after this
  change lands).

## Related Decisions

- **ADR-101**: flat per-value MQTT topics; `sat/room_source` and `sat/room_temp_source` follow it.
- **ADR-199**: SAT cold state; unchanged, it reads the room value this decision selects.
- **ADR-292**: SAT ground truth; the decisions above are Robert's recorded entries.

## References

- TASK-3011, TASK-3015 (backlog/tasks).
- src/OTGW-firmware/SATcontrol.ino: `satGetRoomTemp()`, `satTryRoomSource()`, `satThermostatRoomTemp()`, `satRoomSourceParse()`,
  `satRoomTempSampleMs()`; src/OTGW-firmware/SATble.ino: `satBLEUpdateState()`; src/OTGW-firmware/settingStuff.ino:
  `[sat-blepinonly-migrate]`; src/OTGW-firmware/MQTTHaDiscovery.cpp: `sat_room_temp_source`.
- tests/test_sat_ble_pin_only.py.
- SAT decision register entries 451, 453, 454, 455, 457, 458 (private submodule docs/sat/decisions).
