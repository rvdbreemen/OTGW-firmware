---
id: TASK-1561
title: >-
  REST API for the telnet debug toggles: read and set each flag explicitly (no
  key presses)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-02T13:53:00Z'
updated_date: '2026-11-02T15:04:00Z'
labels:
  - rest-api
  - debug
  - tooling
dependencies: []
priority: medium
ordinal: 713000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02: 'Kan je niet een api call maken voor de debug toggles. Zodat je niet met toets aanslagen hoeft te werken.' Today the debug flags (state.debug.bOTmsg, bRestAPI, bMQTT, bSensors, bSAT, bSATTrace, bOTDirect, bSATBLE, bMQTTGate, bNTP, ...; handleDebug.ino keys 1-8, g, n, ...) can only be flipped over telnet, and every key is a toggle: a script must parse the banner to know the state (scripts/otgw_capture.py parse_toggles, TASK-1453) or it turns a flag off by accident (seen 2026-11-02 in the TASK-1412.6 bench script). Plan: GET /api/v2/debug/flags returns every flag by name; POST /api/v2/debug/flags with {"name":"sat","value":true} sets one flag explicitly (idempotent, same body shape as POST /api/v2/settings), unknown name -> 400. One PROGMEM table maps name -> flag and is shared with the telnet keys so the two cannot drift. Same admin protection as other REST writes (ADR-056). Flags stay transient state (not persisted). New REST contract: Proposed ADR via adr-kit, docs/api update. Then the capture tool and bench scripts can use it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Proposed ADR (adr-kit) for the endpoint: names, body shape, auth, not persisted
- [ ] #2 GET /api/v2/debug/flags lists every telnet debug flag by name with its state; POST sets one flag explicitly and is idempotent; unknown name 400; host test on the real handler (red before, green after)
- [ ] #3 One name->flag table shared by the telnet keys and the REST handler (a test fails when a telnet flag has no REST name)
- [ ] #4 Bench: set SAT debug on/off via REST twice in a row, state as requested both times; docs/api/ updated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. One PROGMEM table in handleDebug.ino (or debugStuff.h): {name, telnet key, bool DebugSection::* member, label} for the 10 log flags: otmsg(1) restapi(2) mqtt(3) sensors(4) sat(5) otdirect(6) satble(7) sattrace(8) mqttgate(g) ntp(n). The telnet keys for these flags read the same table (toggle + the existing 'Debug X: on/off' line), so key and REST name cannot drift. Out of scope (behaviour, not logging, with side effects): 'd' Dallas sensor simulation, 's' OTGW serial simulation (requestOTGWSimulationEvent), and the action keys (r, i, u, j, o, l, f, w, z).
2. REST in handleDebugDump's route (kRouteDebugDump 'debug'): /api/v2/debug/flags. GET -> {"flags":{"otmsg":true,...}} (JsonEmit, no ArduinoJson, ADR-146). POST body {"name":"sat","value":true} (same shape as POST /api/v2/settings; also /api/v2/debug/flags/<name>/<on|off> path form like sat/settings) -> sets the flag explicitly, idempotent, answers the new state; unknown name or non-bool value -> 400. Both behind checkHttpAuth() (Basic Auth + same-origin CSRF, ADR-056), as the GET /api/v2/debug dump already is. Flags stay transient state (not persisted, as today).
3. Proposed ADR via adr-kit (new REST contract), not self-accepted. docs/api/ (REST reference) updated.
4. Tests: host test that compiles the real table + handler logic (set, idempotent, unknown -> 400) and a drift guard: every 'state.debug.b<X> = !state.debug.b<X>' toggle in handleDebug.ino is either in the table or on the explicit out-of-scope list. Red before (no route), green after.
5. Bench .63: POST sat=false twice -> false both times, then true twice -> true; GET matches; telnet key 5 still toggles. Then switch scripts: the TASK-1412.6 bench script and otgw_capture.py (optional follow-up) to the API.
OSS: not applicable (project-specific glue, a few dozen lines).
<!-- SECTION:PLAN:END -->
