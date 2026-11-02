---
id: "ADR-206"
title: "REST API sets the telnet debug log flags by name from one shared table"
status: "Proposed"
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
  - "handleDebugFlags"
  - "handleDebugChar"
  - "DebugSection"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-206 REST API sets the telnet debug log flags by name from one shared table

## Status

Proposed, 2026-11-02.

**Decision Maker:** User: Robert van den Breemen (maintainer) asked for it on 2026-11-02 ("Kan je niet een api call maken voor de debug toggles, zodat je niet met toets aanslagen hoeft te werken", TASK-1561). Drafted by Agent: Claude.

Guideline-level per ADR-080 for the API shape (route, body, answer): no automated gate checks it, code review does. The table drift is gated: `tests/test_debug_flags_api.py` fails when a `DebugSection` bool is neither in the table nor on the exclusion list, when a telnet toggle bypasses the table, or when the REST route skips `checkHttpAuth()`. The declarative rule under Enforcement repeats the bypass check for `bin/adr-judge`.

## Status History

```yaml
status_history:
  - date: 2026-11-02
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal (TASK-1561)
    changed_via: adr-kit
```

## Context

The firmware has ten telnet debug log flags in `state.debug` (`struct DebugSection`, `src/OTGW-firmware/debugStuff.h:47-62`): `bOTmsg`, `bRestAPI`, `bMQTT`, `bSensors`, `bSAT`, `bOTDirect`, `bSATBLE`, `bSATTrace`, `bMQTTGate` and `bNTP`. Each decides whether one area writes its trace to the telnet debug stream on port 23. Until TASK-1561 the only way to change one was a key press on the telnet console (`handleDebugChar()` in `handleDebug.ino`), and every key is a toggle: pressing `5` flips SAT debug, whatever its current value.

There was no REST (Representational State Transfer) API (application programming interface) for them over HTTP (Hypertext Transfer Protocol), so no JSON (JavaScript Object Notation) read-back either.

A toggle is fine for a person who reads the answer. It is fragile for a script. The capture tool (ADR-202) has to parse the telnet banner to learn the current state before it presses a key (`scripts/otgw_capture.py` `parse_toggles`, TASK-1453). A bench script on 2026-11-02 pressed a key to switch a flag on that was already on, and so switched it off. The flags are transient state (ADR-051: `OTGWState`, never persisted), so they reset to their defaults at every boot and a script cannot know them in advance either.

## Decision

The debug log flags get one name table, `src/OTGW-firmware/debugFlags.h` (`kDebugFlags`: REST name, telnet key, `DebugSection` member pointer, telnet label), and both the telnet keys and a new admin REST endpoint `/api/v2/debug/flags` read and set the flags through that table only.

1. **The table.** One line per flag: `{ "sat", '5', &DebugSection::bSAT, "SAT" }`. The telnet handler looks a key up with `debugFlagByKey()` and flips the member (`handleDebug.ino:296-300`); the REST handler looks a name up with `debugFlagByName()` and sets it with `debugFlagSet()`. A new flag needs one table line and gets both interfaces. `bSATBLE` is in the table only when `HAS_SAT_BLE` is set, as its key was before.
2. **Read.** `GET /api/v2/debug/flags` answers `{"flags":{"otmsg":true,"restapi":false,...}}`, one boolean per table line, in table order.
3. **Set, explicitly.** `POST` or `PUT /api/v2/debug/flags` with body `{"name":"sat","value":true}` (the shape of `POST /api/v2/settings`), or the path form `/api/v2/debug/flags/<name>/<value>`. The value is `true`/`false`, `on`/`off` or `1`/`0`; names are case-insensitive. Setting a flag to the value it has is a no-op, so the call is idempotent. An unknown name or a value that is not a boolean answers 400 (Bad Request) and changes nothing. A successful set answers the full flag list, the same body as the GET.
4. **Access.** The route calls `checkHttpAuth()` first (HTTP Basic Auth plus the same-origin CSRF (cross-site request forgery) check, ADR-056), like the existing `GET /api/v2/debug` dump (`restAPI.ino:2164-2205`).
5. **Not persisted.** The flags stay `state.debug` (ADR-051). A REST set lasts until the next boot, exactly like a key press.
6. **Excluded.** The simulation switches `bSensorSim` (telnet `d`, Dallas sensor simulation) and `bOTGWSimulation` (telnet `s`, OTGW serial simulation) are not in the table and not in the API. They change firmware behaviour and have side effects; they stay telnet keys only.
7. **Telnet text unchanged.** A key still prints `Debug <label>: true|false` with the same labels as before, because scripts parse it.
8. **JSON.** The answer is written with JsonEmit (ADR-146); the body is read with the existing `extractJsonField()`. No ArduinoJson.

## Decision Contract

### Must

- Keep every debug log flag of `DebugSection` in `kDebugFlags`, with a unique REST name and a unique telnet key.
- Toggle a log flag from telnet only through `debugFlagByKey()` and the table.
- Set a flag over REST only to an explicit value; answer 400 on an unknown name or a non-boolean value.
- Call `checkHttpAuth()` before any read or write on `/api/v2/debug/flags`.
- Emit the JSON with JsonEmit.

### Must Not

- Persist a debug flag in `settings` or `settings.ini`.
- Offer a toggle verb on the REST API.
- Put `bSensorSim` or `bOTGWSimulation` in the table or the API.
- Change the telnet output text of a flag key.

### Exceptions

- None.

### Verification

- `python tests/test_debug_flags_api.py` (stdlib unittest): compiles `debugFlags.h` against the real `DebugSection` and checks explicit and idempotent sets, rejection without a change, unique names and keys, that every `DebugSection` bool is in the table or on the exclusion list, that no telnet toggle bypasses the table, and that the REST route is authenticated and uses the table.
- On a bench unit: `curl http://<ip>/api/v2/debug/flags`, then `curl -X POST -d '{"name":"sat","value":false}' http://<ip>/api/v2/debug/flags` twice (same answer both times), and the telnet banner shows the same state.

## Alternatives Considered

- **Keep telnet keys only, scripts parse the banner (status quo).** Rejected: every script needs a banner parser (TASK-1453) and a stale or misparsed banner turns a key press into the wrong state, which happened on 2026-11-02.
- **Persist the flags as settings.** Rejected: debug output is transient diagnostics. A verbose flag saved by accident would survive every reboot and keep filling the log, and every change would write `settings.ini` to flash. ADR-051 puts runtime diagnostics in `state`, not in `settings`.
- **A REST "toggle" verb that mirrors the key.** Rejected: it has the same problem as the key (the result depends on the unknown current state) and is not idempotent, so a retried request undoes itself.
- **Include the simulation switches `d` and `s`.** Rejected: they are not logging. `d` re-initialises and polls the Dallas sensors with simulated readings (`handleDebug.ino:403-411`) and `s` feeds simulated OTGW serial events (`handleDebug.ino:412-418`), so a remote switch can change what the boiler and Home Assistant see.
- **A second, REST-only table next to the telnet switch.** Rejected: two lists drift; one table is what lets the drift test exist.

## Consequences

**Positive:**

- Scripts (the capture tool, bench scripts) set the flags they need to a known value in one idempotent call and can read them back as JSON, with no banner parsing.
- One table: a new flag costs one line and appears on telnet and REST at once. The drift test catches a flag that is added to `DebugSection` but not to the table.
- No new dependency and no persisted state.

**Negative:**

- A few hundred bytes of flash for the table, the handler and the name strings (estimate, not measured separately from the TASK-1561 build).
- One more admin route to keep compatible; scripts will depend on the flag names, so a rename is an API change.
- **Open risk:** an admin password is not set by default (ADR-056), and then `checkHttpAuth()` lets every request through. On such a unit any LAN (local area network) client can switch debug output on or off. That is the same exposure as the existing telnet port 23, which has no authentication at all, and it only changes what is logged, not how the boiler is controlled.

## Related Decisions

- **ADR-051 (Settings/State architecture):** the flags are `OTGWState` and stay transient; this ADR does not move them to settings.
- **ADR-056 (Protected admin endpoint security):** the route uses its `checkHttpAuth()` contract, including the default no-password exposure.
- **ADR-146 (No ArduinoJson, JsonEmit):** the answer is built with JsonEmit.
- **ADR-080 (Binding ADR rules need a CI (continuous integration) gate):** the API shape is guideline-level; the table drift is gated by `tests/test_debug_flags_api.py`.
- **ADR-202 (one Python capture tool):** its banner parser is the script-side workaround this API replaces over time.

## References

- Tasks: TASK-1561 (this endpoint), TASK-1453 (capture tool banner parser `parse_toggles` in `scripts/otgw_capture.py`).
- Source: `src/OTGW-firmware/debugFlags.h` (table and helpers), `src/OTGW-firmware/debugStuff.h:47-62` (`DebugSection`), `src/OTGW-firmware/handleDebug.ino:296-300` (telnet keys through the table), `src/OTGW-firmware/restAPI.ino:2164-2205` (`handleDebugFlags`), `restAPI.ino:2210` (dispatch from `/api/v2/debug`).
- Test: `tests/test_debug_flags_api.py`.
- API docs: `docs/api/README.md`.

## Enforcement

The declarative rule catches a telnet handler that flips a log flag by hand instead of through the table; the host test checks the same and the table completeness. The model-based judge is off: the API shape is guideline-level and reviewed at PR.

```json
{
  "forbid_pattern": [
    {
      "pattern": "state\\.debug\\.b(OTmsg|RestAPI|MQTT|Sensors|SAT|OTDirect|SATBLE|SATTrace|MQTTGate|NTP)\\s*=\\s*!",
      "path_glob": "src/OTGW-firmware/handleDebug.ino",
      "message": "Toggle debug log flags through kDebugFlags (debugFlags.h, ADR-206), so telnet and REST share one table."
    }
  ],
  "forbid_import": [],
  "require_pattern": [],
  "llm_judge": false,
  "llm_judge_reason": "The table drift is covered by the declarative rule and tests/test_debug_flags_api.py; the API shape is guideline-level and reviewed at PR."
}
```
