---
id: "ADR-206"
title: "REST API sets the telnet debug log flags by name from one shared table"
status: "Accepted"
date: "2026-11-02"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "debug flags"
  - "telnet debug toggles"
  - "rest api"
  - "diagnostics"
aliases:
  - "debug toggles api"
  - "/api/v2/debug/flags"
  - "kDebugFlags"
  - "debugFlags.h"
components:
  - "Telnet debug console"
  - "REST API v2"
symbols:
  - "kDebugFlags"
  - "DebugFlagDef"
  - "debugFlagSet"
  - "debugFlagWrite"
  - "handleDebugFlags"
  - "handleDebugChar"
  - "DebugSection"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-206 REST API sets the telnet debug log flags by name from one shared table

## Status

Accepted, 2026-11-02.

**Decision Maker:** User: Robert van den Breemen (maintainer) asked for it on 2026-11-02 ("Kan je niet een api call maken voor de debug toggles, zodat je niet met toets aanslagen hoeft te werken", TASK-1561) and, reviewing this record the same day, chose: the flags stay transient, the two simulation switches are included, no extra declarative judge rule. Drafted by Agent: Claude.

Guideline-level per ADR-080 for the API shape (route, body, answer): no automated gate checks it, code review does. The table drift is gated: `tests/test_debug_flags_api.py` fails when a `DebugSection` bool is not in the table, when a telnet toggle bypasses the table, when a simulation side effect runs without a real change, or when the REST route skips `checkHttpAuth()`.

## Status History

```yaml
status_history:
  - date: 2026-11-02
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal (TASK-1561)
    changed_via: adr-kit
  - date: 2026-11-02
    status: Accepted
    changed_by: Robert van den Breemen
    reason: "Accepted by Robert in session 2026-11-02 (AskUserQuestion): flags transient, simulation switches included, no extra judge rule"
    changed_via: adr-kit lifecycle
```

## Context

The firmware has twelve telnet debug flags in `state.debug` (`struct DebugSection`, `src/OTGW-firmware/debugStuff.h:47-62`). Ten are log flags (`bOTmsg`, `bRestAPI`, `bMQTT`, `bSensors`, `bSAT`, `bOTDirect`, `bSATBLE`, `bSATTrace`, `bMQTTGate`, `bNTP`): each decides whether one area writes its trace to the telnet debug stream on port 23. Two are simulation switches: `bSensorSim` (Dallas sensor simulation) and `bOTGWSimulation` (OTGW serial replay). Until TASK-1561 the only way to change any of them was a key press on the telnet console (`handleDebugChar()` in `handleDebug.ino`), and every key is a toggle: pressing `5` flips SAT debug, whatever its current value.

There was no REST (Representational State Transfer) API (application programming interface) for them over HTTP (Hypertext Transfer Protocol), so no JSON (JavaScript Object Notation) read-back either.

A toggle is fine for a person who reads the answer. It is fragile for a script. The capture tool (ADR-202) has to parse the telnet banner to learn the current state before it presses a key (`scripts/otgw_capture.py` `parse_toggles`, TASK-1453). A bench script on 2026-11-02 pressed a key to switch a flag on that was already on, and so switched it off. The flags are transient state (ADR-051: `OTGWState`, never persisted), so they reset to their defaults at every boot and a script cannot know them in advance either.

## Decision

All twelve debug flags get one name table, `src/OTGW-firmware/debugFlags.h` (`kDebugFlags`: REST name, telnet key, `DebugSection` member pointer, telnet label, optional change action), and both the telnet keys and a new admin REST endpoint `/api/v2/debug/flags` read and set the flags through that table only.

1. **The table.** One line per flag: `{ "sat", '5', &DebugSection::bSAT, "SAT", nullptr }`. The telnet handler looks a key up with `debugFlagByKey()` and writes the flipped value with `debugFlagWrite()`; the REST handler looks a name up with `debugFlagByName()` and sets it with `debugFlagSet()`. A new flag needs one table line and gets both interfaces. `bSATBLE` is in the table only when `HAS_SAT_BLE` is set, as its key was before.
2. **Simulation switches included, with their side effects.** `sensorsim` (key `d`) and `otgwsim` (keys `s` and `S`) carry a change action, the same work their telnet keys did before: `sensorsim` re-initialises the Dallas sensors and polls them at once when switched on; `otgwsim` resets the replay timer and signals the replay task (`requestOTGWSimulationEvent`, TASK-1321). `debugFlagWrite()` runs the action only when the value really changes, so setting a switch to the value it has does not restart a replay. Both actions ran from the telnet callback on the async_tcp task before; the REST handler runs on the same task.
3. **Read.** `GET /api/v2/debug/flags` answers `{"flags":{"otmsg":true,"restapi":false,...}}`, one boolean per table line, in table order.
4. **Set, explicitly.** `POST` or `PUT /api/v2/debug/flags` with body `{"name":"sat","value":true}` (the shape of `POST /api/v2/settings`), or the path form `/api/v2/debug/flags/<name>/<value>`. The value is `true`/`false`, `on`/`off` or `1`/`0`; names are case-insensitive. Setting a flag to the value it has changes nothing, so the call is idempotent. An unknown name or a value that is not a boolean answers 400 (Bad Request) and changes nothing. A successful set answers the full flag list, the same body as the GET.
5. **Access.** The route calls `checkHttpAuth()` first (HTTP Basic Auth plus the same-origin CSRF (cross-site request forgery) check, ADR-056), like the existing `GET /api/v2/debug` dump.
6. **Not persisted.** The flags stay `state.debug` (ADR-051). A REST set lasts until the next boot, exactly like a key press.
7. **Telnet text unchanged.** A key still prints `Debug <label>: true|false` with the same labels as before, because scripts parse it.
8. **JSON.** The answer is written with JsonEmit (ADR-146); the body is read with the existing `extractJsonField()`. No ArduinoJson.

## Decision Contract

### Must

- Keep every `DebugSection` bool flag in `kDebugFlags`, with a unique REST name and a unique telnet key.
- Change a flag from telnet only through `debugFlagByKey()`, `debugFlagWrite()` and the table.
- Run a simulation switch's side effect only on a real change of its value.
- Set a flag over REST only to an explicit value; answer 400 on an unknown name or a non-boolean value.
- Call `checkHttpAuth()` before any read or write on `/api/v2/debug/flags`.
- Emit the JSON with JsonEmit.

### Must Not

- Persist a debug flag in `settings` or `settings.ini`.
- Offer a toggle verb on the REST API.
- Change the telnet output text of a flag key.

### Exceptions

- None.

### Verification

- `python tests/test_debug_flags_api.py` (stdlib unittest): compiles `debugFlags.h` against the real `DebugSection` and checks explicit and idempotent sets, rejection without a change, the simulation side effect only on a real change, the `S` key, unique names and keys, that every `DebugSection` bool is in the table, that no telnet toggle bypasses the table, and that the REST route is authenticated and uses the table.
- On a bench unit: `curl http://<ip>/api/v2/debug/flags`, then `curl -X POST -d '{"name":"sat","value":false}' http://<ip>/api/v2/debug/flags` twice (same answer both times), and the telnet key still switches the same flag.

## Alternatives Considered

- **Keep telnet keys only, scripts parse the banner (status quo).** Rejected: every script needs a banner parser (TASK-1453) and a stale or misparsed banner turns a key press into the wrong state, which happened on 2026-11-02.
- **Persist the flags as settings.** Rejected by the maintainer: debug output is transient diagnostics. A verbose flag saved by accident would survive every reboot and keep filling the log, and every change would write `settings.ini` to flash. ADR-051 puts runtime diagnostics in `state`, not in `settings`.
- **A REST "toggle" verb that mirrors the key.** Rejected: it has the same problem as the key (the result depends on the unknown current state) and is not idempotent, so a retried request undoes itself.
- **Leave the simulation switches out (telnet only).** Considered, because they change behaviour rather than logging; the maintainer chose to include them so scripts can drive a simulated run end to end. Their side effects are part of the table entry and run only on a real change.
- **A second, REST-only table next to the telnet switch.** Rejected: two lists drift; one table is what lets the drift test exist.
- **An extra declarative pre-commit rule against a hand-written toggle.** Dropped by the maintainer: the host test already fails on it, and one gate is enough.

## Consequences

**Positive:**

- Scripts (the capture tool, bench scripts) set the flags they need to a known value in one idempotent call and can read them back as JSON, with no banner parsing.
- A simulated bench run (sensor simulation, serial replay) can be started and stopped over REST.
- One table: a new flag costs one line and appears on telnet and REST at once. The drift test catches a flag that is added to `DebugSection` but not to the table.
- No new dependency and no persisted state.

**Negative:**

- A few hundred bytes of flash for the table, the handler and the name strings (estimate, not measured separately from the TASK-1561 build).
- One more admin route to keep compatible; scripts will depend on the flag names, so a rename is an API change.
- The two simulation switches now change firmware behaviour over the network: the serial replay feeds simulated OpenTherm traffic that Home Assistant also sees.
- **Open risk:** an admin password is not set by default (ADR-056), and then `checkHttpAuth()` lets every request through. On such a unit any LAN (local area network) client can switch debug output, and now also the simulations, on or off. Telnet port 23 already allowed the same without any authentication; the simulations never touch how SAT controls the boiler, but a serial replay does show simulated values in Home Assistant.

## Related Decisions

- **ADR-051 (Settings/State architecture):** the flags are `OTGWState` and stay transient; this ADR does not move them to settings.
- **ADR-056 (Protected admin endpoint security):** the route uses its `checkHttpAuth()` contract, including the default no-password exposure.
- **ADR-146 (No ArduinoJson, JsonEmit):** the answer is built with JsonEmit.
- **ADR-080 (Binding ADR rules need a CI (continuous integration) gate):** the API shape is guideline-level; the table drift is gated by `tests/test_debug_flags_api.py`.
- **ADR-202 (one Python capture tool):** its banner parser is the script-side workaround this API replaces over time.

## References

- Tasks: TASK-1561 (this endpoint), TASK-1453 (capture tool banner parser `parse_toggles` in `scripts/otgw_capture.py`), TASK-1321 (the replay event on async_tcp).
- Source: `src/OTGW-firmware/debugFlags.h` (table and helpers), `src/OTGW-firmware/debugStuff.h:47-62` (`DebugSection`), `src/OTGW-firmware/handleDebug.ino` (`handleDebugChar`, `debugFlagSensorSimChanged`, `debugFlagOtgwSimChanged`), `src/OTGW-firmware/restAPI.ino` (`handleDebugFlags`, dispatched from `/api/v2/debug`).
- Test: `tests/test_debug_flags_api.py`.
- API docs: `docs/api/README.md`.

## Enforcement

No declarative rule: the host test `tests/test_debug_flags_api.py` is the gate for the table, the telnet path and the REST route. The model-based judge is off: the API shape is guideline-level and reviewed at PR.

```json
{
  "forbid_pattern": [],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false,
  "llm_judge_reason": "The table drift is covered by tests/test_debug_flags_api.py; the API shape is guideline-level and reviewed at PR."
}
```
