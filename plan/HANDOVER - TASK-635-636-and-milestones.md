# Handover: PV-surplus boost (TASK-635), Modbus TCP client (TASK-636), milestones m-2 and m-3

**Branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support`
**Author of handover:** Claude (chat session, 2026-05-20) for Robert van den Breemen
**Audience:** Claude Code agent picking this up to implement
**Status of attached artefacts:** drafted in chat, NOT YET COMMITTED to repo

-----

## TL;DR for the agent

You have two new backlog tasks to land and two new milestone files to add. The work was scoped in a chat session and is fully drafted below. Your job:

1. **Read `CLAUDE.md` first.** This project has strict task discipline. Direct edits to `backlog/tasks/*.md` are blocked by a `.claude/hooks/backlog-mcp-guard.py` PreToolUse hook. **All task operations go through the `backlog` CLI**, never the MCP server, never direct file edits.
1. **Create TASK-635 and TASK-636 via `backlog task create`** using the content sections below. Do NOT copy the YAML frontmatter from the draft files literally — the CLI generates that. Use the body content (description, acceptance criteria, implementation plan, implementation notes) as input flags to the CLI.
1. **Create the two milestone files directly** in `backlog/milestones/`. The guard hook does not block milestone files (only `backlog/tasks/`). Verify before writing: read `.claude/hooks/backlog-mcp-guard.py` to confirm scope is still `backlog/tasks/`.
1. **Do NOT start implementation of TASK-635 or TASK-636 in this session.** Both tasks need human review of the plan before implementation. Per CLAUDE.md: “Write plan, share with user, WAIT for approval” before coding.

The two tasks are independent from each other in implementation. TASK-635 is scoped for v2.0.0 (label `milestone-v2.0.0`), TASK-636 is deferred to v2.1.0 (labels `milestone-v2.1.0` and `deferred-from-v2.0.0`). TASK-636 has a soft dependency on TASK-635 (narrative and architectural, not blocking).

-----

## Context: why these tasks exist

A comparison analysis between OTGW-firmware v2.0.0 branch and competing projects (Laxilef/OTGateway, ESPHome OpenTherm component, Alexwijn/SAT) identified two gaps worth closing in the OpenTherm + heating-controller space:

1. **PV-surplus aware control** — Laxilef/ESPHome/SAT do not have it. With v2.0.0’s embedded SAT, OTGW-firmware has the architectural position to add it cheaply.
1. **Modbus TCP for heat pump telemetry** — modern heat pumps speak Modbus; OpenTherm coverage for heat pumps is thin. A minimal Modbus TCP client closes this gap without compromising OTGW’s OpenTherm-first scope.

Both gaps were originally raised in a previous analysis but reconsidered after reviewing the v2.0.0 branch. v2.0.0 already adds ESP32, OTDirect, BLE, Ethernet, SSD1306, AP fallback, and embedded SAT. The two tasks below are additive and consistent with that direction.

The full analysis lives in the chat session that produced this handover; key conclusions are folded into the task descriptions and notes below.

-----

## Project conventions you MUST follow

These are extracted from `CLAUDE.md` and `docs/guides/backlog-cli.md` on the `feature-dev-2.0.0-otgw32-esp32-sat-support` branch as of 2026-05-20. **Re-read them in the repo before acting — they may have evolved.**

### Task lifecycle (mandatory)

```bash
# 1. Create the task
backlog task create "Title" -d "Description" \
  --ac "Criterion 1" --ac "Criterion 2" \
  --priority medium \
  -l sat,feature,mqtt,rest,webui,energy,milestone-v2.0.0

# 2. Take ownership (do NOT skip)
backlog task edit <id> -s "In Progress" -a @claude

# 3. Write plan, then STOP. Share with user, wait for approval.
backlog task edit <id> --plan $'1. Step one\n2. Step two\n3. ...'

# 4. During implementation, mark ACs as you go:
backlog task edit <id> --check-ac 1 --check-ac 2
backlog task edit <id> --append-notes $'- Did X\n- Next: Y'

# 5. When done:
backlog task edit <id> --final-summary "PR description text"
backlog task edit <id> -s Done
```

### Hard rules

- **Never use the `mcp__backlog__*` MCP server.** CLAUDE.md explicitly forbids it: it serves stale cached state across the dev/2.0.0 split. Always use the `backlog` CLI directly.
- **Never edit task files in `backlog/tasks/` by hand.** The `.claude/hooks/backlog-mcp-guard.py` PreToolUse hook blocks `Edit/Write/MultiEdit` on that path.
- **Never start coding before writing a plan, sharing it with the user, and waiting for approval.** This is the “Plan-First Development” rule from CLAUDE.md.
- **One task per piece of work.** No “while I’m here” creep. Open a follow-up task instead.

### PR readiness

Before marking a task `Done`, run through `docs/guides/pr-checklist.md`. Baseline gates that apply to most firmware changes:

```bash
python build.py                    # Builds ESP8266 and ESP32-S3
python evaluate.py --quick         # Health score must be ≥ previous commit
python tests/test_evaluate.py      # Stdlib unittest suite
python tests/test_build.py
```

For firmware-behavior changes that humans can only verify on real hardware, the task notes must include a `## Test Plan` section. Both tasks below have one in their Implementation Notes — preserve and extend during implementation.

-----

## Artefact 1: TASK-635 — SAT PV-surplus aware setpoint boost

### Metadata to pass to `backlog task create`

- **Title:** `SAT: PV-surplus aware setpoint boost (opportunistic heat absorption)`
- **Priority:** `medium`
- **Labels:** `sat`, `feature`, `mqtt`, `rest`, `webui`, `energy`, `milestone-v2.0.0`
- **Dependencies:** none
- **Assignee on take-over:** `@claude`

### Description (use as `-d` argument)

```
SAT already accepts external indoor and outdoor temperature via MQTT/REST (satHandleExternalTemp, satHandleExternalOutdoor). With those inputs, a user can build PV-surplus aware behavior entirely in Home Assistant by feeding a fake "warmer outdoor" value, but that is a hack: it pollutes the outdoor sensor history, it breaks heating-curve recommendation statistics, and it does not survive HA reboots.

This task adds PV-surplus as a first-class SAT input: a new optional surplus power input (W) that, when above a threshold and confirmed for a hold-time, opportunistically boosts the SAT target temperature by a configurable delta. This stores excess PV energy in the home as thermal mass (radiator water and structure), giving the user energy savings without compromising the heating-curve model or the safety system.

Scope is intentionally minimal for v2.0.0: a target-temperature boost driven by an external surplus signal, with safety constraints (max boost, max indoor temp ceiling, hysteresis on threshold). No native PV measurement, no inverter integration. The feature complements rather than replaces HA-side automations, and degrades gracefully when no surplus input arrives (stale-input expiry, same pattern as bExternalTempValid).

Rationale: the feature is low-hanging fruit because the integration point (target temp boost) already exists in code (settings.sat.fTargetTemp is reassigned by the preset/window logic at runtime), and the input pattern is identical to the existing external temp handlers. It differentiates the firmware against Laxilef and ESPHome (neither has this), and aligns with the prosumer/energy-transition user profile.
```

### Acceptance criteria (pass each as `--ac "..."`)

1. New settings in `SATSection` struct: `bPvBoostEnabled` (bool, default false), `iPvBoostThresholdW` (uint16, default 1500, range 100-10000), `iPvBoostHoldS` (uint16, default 120, range 30-600), `fPvBoostDeltaC` (float, default 1.5, range 0.5-5.0), `fPvBoostMaxIndoorC` (float, default 23.0, range 18.0-28.0)
1. New runtime state in `SATState`: `fExternalPvSurplusW` (float), `bExternalPvSurplusValid` (bool), `iExternalPvSurplusLastMs` (uint32), `bPvBoostActive` (bool), `iPvBoostStartedMs` (uint32), `fPvBoostAppliedC` (float)
1. New external input handler `satHandlePvSurplus(const char* value)` in `SATcontrol.ino`, following the exact pattern of `satHandleExternalTemp`: numeric validation, range check (0 to 50000 W), timestamp update, debug log
1. Stale-input expiry: PV surplus value invalidates after `settings.sat.iSensorMaxAgeS` seconds (reuse existing setting, do not add a new one). Boost deactivates immediately on invalidation
1. Boost activation logic in the SAT control loop: surplus > threshold for ≥ hold-time AND current indoor temp < boost-max-indoor → activate boost, set `fPvBoostAppliedC = fPvBoostDeltaC`. Surplus < threshold (with 20% hysteresis, i.e. < 0.8 * threshold) OR indoor temp ≥ boost-max-indoor → deactivate boost immediately
1. Effective target temp = `settings.sat.fTargetTemp + state.sat.fPvBoostAppliedC` (when active). Implementation must NOT mutate `settings.sat.fTargetTemp`; the boost is applied at the point where target is read by the heating curve and PID (around lines 820, 3104, 3644 in SATcontrol.ino as of 2026-05-20 — verify line numbers at implementation time). Confirm presets, window detection, and activity preset still operate on the unboosted target
1. Boost is bounded: never apply boost when window detection has cleared the target, never apply when SAT safety is tripped, never apply when boiler is in DHW priority
1. MQTT subscribe: `set/<nodeId>/sat/pv_surplus_w` (numeric W), `set/<nodeId>/sat/pv_boost_enabled` (0/1)
1. MQTT publish in `satPublishMQTT()`: `sat/pv_surplus_w`, `sat/pv_surplus_valid` (0/1), `sat/pv_boost_active` (0/1), `sat/pv_boost_applied_c`, plus all five new settings with the `sat/pv_boost_*` prefix
1. REST API: new settings visible in `GET /api/v2/settings` (keys `satpvboostenabled`, `satpvboostthresholdw`, `satpvboostholds`, `satpvboostdeltac`, `satpvboostmaxindoorc`). New endpoint `POST /api/v2/sat/pvsurplus` with body `{"value": <W>}`, mirroring `POST /api/v2/sat/externaltemp`
1. REST API: `GET /api/v2/sat/status` JSON includes `pv_surplus_w`, `pv_surplus_valid`, `pv_boost_active`, `pv_boost_applied_c`
1. HA auto-discovery: PV surplus W as `sensor` (unit W, device class power), `pv_boost_active` as `binary_sensor`, `pv_boost_applied_c` as `sensor` (unit °C). Boost enable as `switch`, the four numeric settings as `number` entities with min/max from the setting ranges
1. OpenAPI 3.0 spec (`docs/api/openapi.yaml`) updated with the new POST endpoint and status fields. Coordinated with TASK-210 (SAT REST endpoints to openapi.yaml) if still open
1. WebUI: PV-boost section in SAT settings page (5 fields, with inline help: what the threshold means, what the hold-time prevents, why the indoor max matters as safety ceiling). Default state: collapsed/disabled until user enables
1. WebUI SAT dashboard: when boost is active, show a clear visual indicator (badge “PV boost: +1.5°C active”) next to the target temperature. When inactive but enabled and waiting, show “PV boost: armed (surplus 800W < 1500W threshold)”
1. Documentation: new section in `docs/features/smart-autotune-thermostat.md` explaining the feature, intended use case (radiator/UFH heat absorption from PV), limits (not a DHW boost — that needs separate logic), and a HA automation example that publishes `pv_surplus_w` from an existing PV sensor (e.g. solar inverter integration or P1 meter calculation)
1. NL manual chapter `docs/manuals/nl/h05-sat-thermostaat.md` and EN equivalent updated with the new feature
1. ADR: new ADR-104 (or next free number at implementation time) “SAT PV-surplus opportunistic setpoint boost” documenting the design decision (additive boost, no mutation of `settings.sat.fTargetTemp`, hysteresis percentage rationale, why no DHW boost in this iteration, why reuse `iSensorMaxAgeS` for stale-input expiry)
1. Default behavior: with `bPvBoostEnabled = false` (the default), this feature is completely inert. No new MQTT publishes when disabled (only the enable flag is published). Existing users see no change after upgrade
1. Boost-active duration cap: maximum continuous boost duration is bounded. Add `iPvBoostMaxDurationMin` setting (uint16, default 240, range 30-1440). After cap, deactivate boost and refuse to reactivate for 30 minutes (cooldown). Prevents runaway boost on faulty surplus signal

### Implementation plan (pass as `--plan` after `-s "In Progress"`)

```
1. Add six new settings to SATSection struct in OTGW-firmware.h (bPvBoostEnabled, iPvBoostThresholdW, iPvBoostHoldS, fPvBoostDeltaC, fPvBoostMaxIndoorC, iPvBoostMaxDurationMin)
2. Add seven new runtime state fields to SATState struct in OTGW-firmware.h (fExternalPvSurplusW, bExternalPvSurplusValid, iExternalPvSurplusLastMs, bPvBoostActive, iPvBoostStartedMs, fPvBoostAppliedC, iPvBoostCooldownUntilMs)
3. Add settings persistence in settingStuff.ino (write/read/update with constrain), following the existing pattern for SAT settings
4. Implement satHandlePvSurplus() in SATcontrol.ino adjacent to the existing external handlers (around line 1080), with identical validation pattern
5. Implement satEvaluatePvBoost() function called from the SAT control loop (next to satApplyPresets() or satCheckWindow()). This function: checks enable flag; checks input validity (stale expiry); evaluates threshold + hold + indoor max + duration cap + cooldown; updates bPvBoostActive and fPvBoostAppliedC
6. Introduce a satGetEffectiveTarget() helper returning settings.sat.fTargetTemp + (state.sat.bPvBoostActive ? state.sat.fPvBoostAppliedC : 0.0f). Replace direct reads of settings.sat.fTargetTemp at the three identified call sites in SATcontrol.ino with this helper. Audit all other reads of fTargetTemp to confirm preset/window logic is untouched (those should NOT use the effective getter)
7. Verify safety interactions: if state.sat.bSafetyTripped is true, force bPvBoostActive = false. If window is open (target overridden), boost is suspended. If boiler is in DHW priority, boost is suspended
8. Add MQTT subscribe handlers in MQTTstuff.ino for pv_surplus_w and pv_boost_enabled (and the four numeric setting topics following the existing pattern for SAT settings)
9. Extend satPublishMQTT() with the new publishes. Gate publishes when feature is disabled per AC #19
10. Add REST endpoint POST /api/v2/sat/pvsurplus in restAPI.ino mirroring the existing POST /api/v2/sat/externaltemp
11. Extend the SAT status JSON output with the four new fields
12. Add HA auto-discovery entries: one sensor, one binary_sensor, one sensor (applied_c), one switch, five numbers
13. WebUI: extend sat.js to render boost status and applied delta in the dashboard. Extend index.js/index.html with the new settings rows. Keep the section visually grouped under "PV Surplus Boost" with clear labels
14. Update OpenAPI spec, feature documentation (EN + NL manuals), and write ADR-104
15. Manual testing: enable feature; publish synthetic surplus values via MQTT; confirm boost activates after hold-time, deactivates on threshold breach with hysteresis, deactivates on indoor ceiling breach, deactivates on safety trip, respects duration cap and cooldown, and survives a SAT disable/enable cycle (fPvBoostAppliedC must reset to 0)
```

### Implementation notes (append after plan approval, before coding)

**Design rationale**

- The boost is applied at target-temperature read sites, not at the heating-curve output. The entire SAT control chain (heating curve, PID, cycle classifier, safety) then operates on the boosted target consistently. Applying at the curve output would cause PID to fight the curve and the cycle classifier to flag the resulting overshoot.
- `settings.sat.fTargetTemp` is deliberately NOT mutated. The preset/window logic already mutates it with `fPreCustomTemp` / `fPreWindowTarget` rollback fields; adding a third mutator would require triple bookkeeping. An additive boost via `satGetEffectiveTarget()` is simpler and safer.
- Hysteresis is set at 20% of the threshold to prevent flapping when a PV inverter produces noisy values around the threshold (partly cloudy days). The hold-time prevents activation on transient peaks (cloud edge effect). Together they should give a few activate/deactivate cycles per day on a typical mixed-weather day.
- The indoor max ceiling (default 23.0°C) is a comfort safety, not a hardware safety. The existing SAT hard temperature ceiling (50°C UFH / 80°C radiator) still bounds the flow temperature. The indoor max prevents the boost from cooking the user.
- The duration cap (default 4 hours) plus 30-minute cooldown prevents two failure modes: (1) a faulty surplus sensor reporting 5000W continuously, and (2) a thermal-mass-saturated home where additional heat input no longer helps.

**Why not native PV measurement**

OTGW32 has GPIO headroom in theory, but adding a current-clamp ADC reader, calibration logic, and a single/three-phase model would more than double the scope. Users with PV already have an inverter integration in HA or a P1-meter reader. Trusting that input is consistent with how SAT already trusts MQTT-supplied indoor/outdoor temps.

**Why not DHW boost in this iteration**

DHW is controlled via `hotwater` MQTT commands (PIC OT command `HW=`) and SAT’s role for DHW is minimal. A “boost DHW setpoint on PV surplus” feature would touch a different control path and warrant its own task.

**HA automation example for docs**

```yaml
automation:
  - alias: "Publish PV surplus to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.solar_net_power
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw/sat/pv_surplus_w"
          payload: "{{ states('sensor.solar_net_power') | float }}"
```

Where `sensor.solar_net_power` is positive when exporting to grid.

**Test Plan** (required for firmware-behavior changes per pr-checklist.md)

1. Cold sunny day with mixed clouds: expect 3-5 boost cycles, no flapping. Verify via MQTT log of `sat/pv_boost_active`.
1. PV inverter offline mid-day: expect surplus invalidation after `iSensorMaxAgeS`, boost deactivates within one control interval.
1. User toggles SAT off while boost is active: expect `fPvBoostAppliedC` to reset to 0.
1. Window detection triggers while boost is active: expect boost to suspend, then resume when window closes (if surplus still present and hold-time re-elapses).
1. Boost reaches 4-hour duration cap: expect deactivation, 30-minute cooldown, no re-activation during cooldown even if surplus persists.
1. Indoor temp drifts up to 23°C while boost is active (and surplus is still high): expect deactivation on indoor ceiling.
1. PV sensor faulty, publishing 9999W constantly: expect duration cap to cut off the boost; user sees in dashboard.

**Coordination notes for the agent**

- AC #18 ADR may need renumbering if PRs merge in parallel. Check next free number at PR open time (current expectation: ADR-104).
- AC #13 OpenAPI work overlaps with TASK-210 if that task is still in progress. Coordinate to avoid merge conflicts.
- The new `iPvBoostMaxDurationMin` setting reuses the SAT settings persistence pattern. Validate flash footprint on ESP8266 build (Arduino Core 2.7.4) before merging — six new fields is the first time this struct grows since the last audit.

-----

## Artefact 2: TASK-636 — Modbus TCP client (deferred to v2.1.0)

### Metadata to pass to `backlog task create`

- **Title:** `Modbus TCP client for heat pump integration (ESP32-only, experimental)`
- **Priority:** `low`
- **Labels:** `feature`, `esp32-only`, `experimental`, `modbus`, `heatpump`, `energy`, `milestone-v2.1.0`, `deferred-from-v2.0.0`
- **Dependencies:** `TASK-635`
- **Assignee on take-over:** unassigned (do not assign `@claude` yet; this task should not be picked up until TASK-635 has landed and v2.0.0 has shipped)

### Description (use as `-d` argument)

```
OTGW-firmware is OpenTherm-centric by design, and that scope is deliberate. However, the heat pump market is moving to Modbus as the primary control and telemetry protocol, while OpenTherm coverage for heat pumps remains thin. This task adds a minimal, optional, ESP32-only Modbus TCP client that polls a configurable register map from a heat pump on the local network. The client publishes register values to MQTT (with HA auto-discovery) and exposes a small write surface for setpoint changes. RTU/RS485 is deliberately out of scope for this iteration: it requires hardware (MAX485 transceiver, wiring) and substantially expands testing surface. TCP-only keeps the scope to "another network client", same as MQTT or NTP.

The feature is positioned as experimental and opt-in:
- Disabled by default
- ESP32 only (compile-time excluded on ESP8266 due to memory pressure)
- Vendor-agnostic: users supply their own register map via a JSON config file on LittleFS
- No vendor-specific code in firmware; vendor profiles live as community-contributed JSON files in docs/features/modbus-profiles/
- Polling read + small write surface only; no flow control, no master mode, no scan/discovery

The strategic intent is to acknowledge the heat-pump direction without committing to heat-pump-first redesign. OpenTherm remains the primary control path. Modbus is a side channel for telemetry and limited write access on systems where OpenTherm coverage is insufficient.

This task does NOT wire Modbus into SAT's control loop. SAT continues to operate purely on OpenTherm and external sensor inputs. A future task may introduce Modbus-derived inputs into SAT, but that decision is deferred until this base feature is mature.

Pairs with TASK-635 (PV-surplus boost): typical user flow becomes "P1 meter → HA → MQTT publishes pv_surplus_w to SAT → SAT boosts target → heat pump runs harder on PV", while OTGW separately polls the heat pump via Modbus for visibility and exposes a "DHW setpoint boost on surplus" write that HA can trigger.
```

### Acceptance criteria (pass each as `--ac "..."`)

1. New compile-time guard: entire Modbus subsystem is wrapped in `#if defined(ESP32)`. ESP8266 build produces identical binary as without this task (verify via flash/RAM size diff)
1. New settings in a dedicated `ModbusSection` struct (separate from SAT): `bEnabled` (bool, default false), `sHost` (char[64]), `iPort` (uint16, default 502), `iUnitId` (uint8, default 1, range 0-247), `iPollIntervalS` (uint16, default 30, range 5-3600), `iTimeoutMs` (uint16, default 1000, range 100-5000), `iMaxConsecutiveFailures` (uint8, default 5, range 1-20)
1. Register map loaded from `/modbus.json` on LittleFS at boot. JSON schema documented in `docs/features/modbus-tcp-client.md`. Schema fields per register: `name`, `address`, `type` (u16/s16/u32/s32/float/bool), `byte_order` (AB/BA/ABCD/BADC/CDAB/DCBA, default AB), `function` (holding/input, default holding), `scale` (default 1.0), `offset` (default 0.0), `unit`, `device_class` (optional), `writable` (default false)
1. Register map limits: maximum 32 registers per map. JSON parse failure → Modbus subsystem stays disabled at runtime with clear error in debug log, MQTT publishes `modbus/status = config_error`. No crash, no boot loop
1. Modbus library selection: use `eModbus` or `emelianov/modbus-esp8266` (despite the name, supports ESP32). Decision documented in ADR (see AC #18). Choose based on: maintenance activity, TCP client support, license (MIT/BSD), code size on ESP32
1. Polling loop: ESP32 task (separate FreeRTOS task, stack ~4KB) polls all configured registers at `iPollIntervalS` interval. Connection lifecycle: open at start of poll cycle, read in batches, close. Persistent connections deferred to future task
1. Connection failure handling: after `iMaxConsecutiveFailures` consecutive failed polls, mark subsystem failed, publish `modbus/status = failed`, back off to 5-minute retry. Successful poll resets failure counter
1. MQTT subscribe for writable registers: `set/<nodeId>/modbus/<register_name>` accepts numeric payload. Written via function code 6 or 16. Result published to `modbus/<register_name>/write_result` (`ok` / `failed` / `not_writable` / `range_error`)
1. MQTT publish in new `modbusPublishMQTT()` function: each register as `<topPrefix>/value/<nodeId>/modbus/<register_name>` with scaled+offset value. Publish on change + heartbeat per existing publish-interval setting (reuse gating, do not invent new)
1. MQTT status topics: `modbus/status` (disabled/config_error/connecting/ok/failed), `modbus/last_poll_ms`, `modbus/consecutive_failures`
1. REST API: `GET /api/v2/modbus/status`, `GET /api/v2/modbus/registers` (all current values as JSON map), `POST /api/v2/modbus/register/<name>` (write)
1. REST API: `GET /api/v2/modbus/config` returns loaded map JSON. `POST /api/v2/modbus/config` accepts new JSON map, validates, persists, reloads
1. HA auto-discovery: each register as `sensor` (or `binary_sensor` for type=bool). Writable registers additionally as `number` entities with unit/device_class. Subsystem status as `sensor`
1. WebUI: new “Modbus” page (visible only on ESP32 builds), with connection settings section and register map editor (read-only view + file upload). Status panel shows connection state, last poll, failures, per-register value table updated via REST poll
1. OpenAPI 3.0 spec updated with new endpoints
1. Documentation: new file `docs/features/modbus-tcp-client.md` with schema for `modbus.json`, byte-order explanation, polling/connection model, security considerations (LAN-only, no auth), supported data types, worked example for generic heat pump
1. Community register map collection: new directory `docs/features/modbus-profiles/` with `README.md` explaining contribution flow. Seeded with one example profile labeled “community-contributed, not vendor-endorsed”. License of profile JSON files: MIT
1. ADR: new ADR (next free number after TASK-635’s ADR-104) “Modbus TCP client as optional ESP32 side channel” documenting: why Modbus given OpenTherm-first scope, why TCP not RTU, why vendor-agnostic register map not vendor-specific code, why no SAT integration this iteration, library choice rationale, security model
1. NL manual chapter `docs/manuals/nl/h11-modbus-tcp.md` (or next free chapter number) and EN equivalent describing feature, configuration steps, heat pump use case. Both manuals clearly mark as experimental
1. Default behavior: with `bEnabled = false` (default), Modbus task not started, no FreeRTOS task, no LittleFS file read, no MQTT topics published. Zero runtime cost when disabled
1. Resource budget when enabled: total RAM footprint (task stack + register state + connection buffer) < 12KB on ESP32. Flash impact < 50KB. Validate before merge
1. Safety: no register write occurs without explicit user action (MQTT publish, REST call, WebUI button). No autonomous writes from firmware logic. SAT may NOT write to Modbus registers in this task (deferred to future task)

### Implementation plan (pass as `--plan` when picked up — NOT in this session)

```
1. Library evaluation spike (½ day): compare eModbus vs emelianov/modbus-esp8266 on ESP32. Build small test sketch with each, measure flash/RAM footprint, test against diagslave (libmodbus) or a Python pymodbus server. Document choice in ADR per AC #18
2. Add ModbusSection struct in OTGW-firmware.h or new Modbustypes.h (consistent with SATtypes.h pattern). Wrap in #if defined(ESP32). Add settings persistence in settingStuff.ino
3. Implement JSON register map loader in new file src/OTGW-firmware/Modbusconfig.ino. Parse /modbus.json from LittleFS, validate schema, build in-memory register table. Handle all four failure modes with clear status reporting
4. Implement polling task in new file src/OTGW-firmware/Modbusclient.ino. xTaskCreate on ESP32, stack 4KB, priority below WiFi. Task body: sleep, connect, read, close, publish. Use FreeRTOS notifications for write requests so writes don't block on poll cycle
5. Implement byte-order decoder for the six byte-order modes per AC #3. Cover with unit tests in tests/ if infrastructure permits — this is where 80% of Modbus integration bugs live
6. MQTT subscribe in MQTTstuff.ino: handler for set/<nodeId>/modbus/<register_name>. Lookup register, validate writability, range-check, dispatch via FreeRTOS notification
7. MQTT publish via new modbusPublishMQTT() called from Modbus task after successful poll. Use existing publish-interval gating
8. REST endpoints in restAPI.ino: five new endpoints per AC #11 and #12. Config upload reuses existing LittleFS upload pattern
9. HA auto-discovery: extend discovery framework with Modbus entities. Published once at subsystem-start and on register map reload. Honor retained-discovery orphan cleanup (ADR-093) when registers removed
10. WebUI Modbus page: conditional render based on /api/v2/device/info reporting ESP32. File upload via existing file-manager primitives. Status panel polls /api/v2/modbus/status every 5s
11. OpenAPI spec, ADR, EN+NL manuals
12. Sample register map docs/features/modbus-profiles/example-generic-heatpump.json with 6-8 commonly-exposed registers
13. Manual testing: against pymodbus simulator first, then against real heat pump from community (Ochsner or Kronoterm via volunteer testing). Validate the seven scenarios in notes
```

### Implementation notes

**Targeted at v2.1.0, deferred from v2.0.0**

Three reasons:

1. v2.0.0’s test/validation surface is already large (two MCUs, two OT paths, BLE, Ethernet, SAT, OLED, AP fallback). Adding a new networking subsystem before v2.0.0 ships amplifies launch risk for a feature benefiting a minority of users.
1. The library-evaluation spike (AC #5) needs to complete before any implementation. Doing this under v2.0.0 timeline pressure forces a suboptimal choice.
1. TASK-635 is the better v2.0.0 candidate from the same feature family: smaller surface, no new dependencies, demonstrates the “OTGW responds to external energy signals” pattern. Landing it first sets the narrative; this task extends it later.

**Why a separate task from TASK-635**

TASK-635 is purely additive to existing SAT external-input handlers. This task introduces a whole new subsystem with networking, persistence, MQTT topic tree, HA discovery, WebUI page. Mixing would conflate two scope decisions: (1) “should SAT respond to PV?” — clearly yes; (2) “should OTGW speak Modbus?” — carefully, optionally, with clear boundaries.

**Why not Modbus RTU in this iteration**

RTU requires: MAX485 transceiver wiring, GPIO conflicts with OTDirect/OLED, termination resistor decisions, baud/parity/stop bit config, half-duplex bus timing. Each is solvable but compounds testing surface dramatically. TCP is “another TCP client like MQTT” and ships in weeks; RTU is a hardware feature and ships in months.

**Why vendor-agnostic register maps**

Vendor register maps can have 200+ registers, change between firmware versions, be partially undocumented. Hardcoding per-vendor logic creates an endless maintenance tail. JSON profiles in `docs/features/modbus-profiles/` let community own vendor-specific knowledge while firmware stays the engine. Same model as Tasmota templates.

**Why no SAT-to-Modbus write in this task**

If SAT writes setpoints to a heat pump while also driving an OpenTherm boiler, two control authorities can fight on the same physical device. That requires a coordination model (which authority wins when, what happens on Modbus failure, how does PIC OT command queue interact with the Modbus task). That’s a design problem deserving its own task with its own ADR. This iteration is deliberately read-mostly + user-triggered-write.

**Security model**

Modbus TCP has no authentication; this is protocol design, not configurable. Document clearly: do not enable on networks with untrusted devices. Firmware’s role is transparency about this, not adding a pseudo-auth layer that would not interop with any real heat pump. Consistent with OTGW’s existing LAN-trusted security model.

**Library choice prediction (subject to AC #5 spike)**

`eModbus` looks more actively maintained (commits in 2025-2026, async architecture, TCP and RTU support) but has a larger footprint. `emelianov/modbus-esp8266` is smaller and simpler but lower maintenance velocity. Current expectation: eModbus wins on long-term viability, modbus-esp8266 wins on minimal-footprint. Decide on the spike.

**Relationship to OTDirect**

OTDirect uses GPIO for OpenTherm bus on ESP32. Modbus TCP uses no GPIOs (pure network). No conflict. If future RTU follow-up happens, that DOES conflict with OTDirect on GPIO budget — OTDirect wins, it’s core.

**Out of scope for this task**

- Native PV current/power measurement (see TASK-635 notes — users supply via HA)
- SAT-controlled heat pump setpoint writes (deferred, requires coordination ADR)
- Modbus RTU over RS485 (deferred, hardware feature)
- Modbus slave/server mode (different use case)
- Vendor-specific protocol extensions (NIBE modbus40 framing, KNX-over-Modbus) — community territory

**Test Plan** (required for firmware-behavior changes per pr-checklist.md)

1. Heat pump online, register map loads, all registers polled successfully → MQTT topics populate, HA entities appear
1. Heat pump goes offline mid-day → failure counter increments, after 5 failures status flips to `failed`, retry every 5 min, automatic recovery on return
1. Bad `modbus.json` (syntax error) → subsystem stays disabled, clear error in debug log and `modbus/status`, no boot loop
1. Register map exceeds 32 registers → load fails with `config_error`
1. Write to writable register via MQTT → register updates on device, value re-read on next poll matches
1. Write to non-writable register → `not_writable` response, no Modbus write attempted
1. Subsystem disabled (default) → zero new MQTT topics, zero FreeRTOS tasks, identical behavior to v2.0.0 without this task
1. Replace register map via WebUI upload → old HA entities cleaned up via retained-discovery orphan cleanup (ADR-093), new entities appear

-----

## Artefact 3: Milestone files

These are NOT task files and are NOT blocked by the PreToolUse hook. Write them directly with `Write` or `Edit` tools.

**Verify before writing:** read `.claude/hooks/backlog-mcp-guard.py` to confirm scope is still `backlog/tasks/` only. If the guard has been extended to cover `backlog/milestones/`, switch to the same `backlog` CLI flow if available, or escalate to user.

### File: `backlog/milestones/m-2 - v2.0.0.md`

```markdown
---
id: m-2
title: "v2.0.0 Release"
---

## Description

Major platform-release. Brengt de firmware van een ESP8266-laag op de NodoShop OTGW PIC naar een complete heating controller met ESP32/OTGW32 support, OTDirect (OpenTherm zonder PIC), W5500 Ethernet, BLE room sensors, SSD1306 OLED display, AP fallback mode (beta), en SAT (Smart Autotune Thermostat) als embedded controller geport vanuit Alexwijn/SAT.

Deze milestone is een release-marker, geen werkstroom. De twee onderliggende werkstromen die deze release voeden zijn:

- `m-0` Audit SAT C++ (volledige audit van de Python SAT naar C++ port).
- `m-1` OTGW32 Support (audit en validatie van de OTGW32/OT-Thing port).

Tasks die direct in v2.0.0 landen krijgen het label `milestone-v2.0.0`. De m-0 en m-1 audit-tasks hebben hun eigen audit-specifieke labels en hoeven niet apart op v2.0.0 te worden gelabeld (de milestone-relatie is impliciet via hun audit-context).

### Scope-richtlijnen

1. **Platform-foundation eerst**. ESP32/OTGW32, OTDirect, BLE, Ethernet, SAT-port en AP fallback vormen samen het fundament. Alles wat daar afhankelijk van is heeft hogere prioriteit dan losstaande features.
2. **Backwards compatibility voor ESP8266 gebruikers**. Bestaande v1.x gebruikers op NodoShop OTGW hardware moeten kunnen upgraden zonder gedragsverandering. Eén bewuste MQTT-topic breaking change is geaccepteerd (de drie OT-bus presence topics, gedocumenteerd in ADR-084) met self-healing migratie.
3. **Documentatie- en governance-discipline behouden**. ADRs, OpenAPI, MQTT reference, EN+NL manuals blijven op niveau.

### Niet-audit tasks gelabeld voor v2.0.0

- TASK-635: SAT PV-surplus aware setpoint boost (opportunistic heat absorption). Laag-risico additieve feature op bestaande SAT external-input pattern; vestigt de "OTGW reageert op externe energiesignalen" narrative die TASK-636 (Modbus, in v2.1.0) later extends.

Audit-tasks onder `m-0` en `m-1` zijn separaat gelabeld en vormen het bulk-werk van deze milestone; zie die milestones voor hun task-lijsten.

### Uitgesteld naar v2.1.0

- TASK-636: Modbus TCP client for heat pump integration. Doorgeschoven om launch-risico te beperken en de library-evaluatie zonder tijdsdruk uit te kunnen voeren. Zie `m-3 - v2.1.0` voor de uitstel-rationale.
```

### File: `backlog/milestones/m-3 - v2.1.0.md`

```markdown
---
id: m-3
title: "v2.1.0 Release"
---

## Description

Eerste minor release na de v2.0.0 platform-release. Doel: stabilisatie van wat in v2.0.0 geland is, plus gerichte feature-uitbreidingen die bewust uit v2.0.0 zijn gehouden om de launch-scope beheersbaar te houden.

Scope-richtlijnen voor deze milestone:

1. **Stabilisatie eerst**. Bugfixes en hardening op de v2.0.0 platform-features (ESP32/OTGW32, OTDirect, BLE, Ethernet W5500, SAT embedded port, AP fallback) hebben voorrang boven nieuwe features. AP fallback uit beta halen valt expliciet binnen deze milestone.
2. **Nieuwe features zijn opt-in en gating-by-default**. Geen feature in v2.1.0 mag bestaande v2.0.0 gebruikers gedrag-aanpassingen opleveren bij upgrade zonder expliciete activering.
3. **Test- en validatie-investering** verder uitbouwen. v2.0.0's combinatorische test matrix (twee MCUs × twee OT paden × meerdere sensorbronnen) moet in v2.1.0 met geautomatiseerde rookjes per platform afgedekt zijn.
4. **UI en onboarding polish** waar mogelijk meeliften, zonder dat het een UI-redesign milestone wordt.

Geplande taken voor v2.1.0 worden gelabeld met `milestone-v2.1.0`. Taken die uit v2.0.0 zijn doorgeschoven krijgen daarnaast het label `deferred-from-v2.0.0` zodat de reden van uitstel terugvindbaar blijft.

### Initiële task-set

- TASK-636: Modbus TCP client for heat pump integration (ESP32-only, experimental). Uitgesteld vanuit v2.0.0 om launch-risico te beperken en de library-evaluatie zonder tijdsdruk uit te kunnen voeren.

Aanvullende taken worden toegevoegd op basis van community-feedback na v2.0.0 release en de uitkomsten van de v2.0.0 stabilisatie-cyclus.

### Niet in scope voor v2.1.0

- Modbus RTU over RS485 (hardware feature, eigen task wanneer TCP-pad volwassen is).
- Modbus slave/server mode (andere use case).
- SAT-naar-Modbus write integratie (vereist coördinatie-ADR over twee control-autoriteiten op één fysiek apparaat; eigen task in latere milestone).
- Native PV current/power measurement op ESP32 (out of scope, gebruikers leveren via HA aan).
- YAML/DSL configuratie layer in ESPHome-stijl (geen strategisch doel).
```

-----

## Agent execution checklist

In order. Each step is a hard gate; do not proceed past a failed step.

1. **[Read]** `cat CLAUDE.md` — confirm task discipline is unchanged from this handover (Plan-First, no MCP, no direct task-file edits).
1. **[Read]** `cat docs/guides/backlog-cli.md` — confirm CLI flags used below are still current.
1. **[Read]** `cat .claude/hooks/backlog-mcp-guard.py` — confirm scope is still `backlog/tasks/` only (not `backlog/milestones/`).
1. **[Verify]** Branch is `feature-dev-2.0.0-otgw32-esp32-sat-support`: `git branch --show-current`. If not, switch.
1. **[Verify]** Next free task ID. Run `ls backlog/tasks/ | grep -oE 'task-[0-9]+' | sort -V | tail -3`. If the next free IDs are NOT 635 and 636, **stop and ask the user how to renumber**. Do not invent new IDs without confirmation.
1. **[Verify]** Next free ADR number for AC #18 of TASK-635. Run `ls docs/adr/ | grep -oE 'ADR-[0-9]+' | sort -V | tail -3`. Update the AC text if 104 is taken.
1. **[Create]** TASK-635 via `backlog task create`. Use the title, description (multiline via `$'...\n...'`), all 20 ACs as separate `--ac` flags, priority `medium`, labels as listed. Do NOT assign yet.
1. **[Verify created]** `backlog task 635 --plain` — confirm content matches. If anything missing or wrong, `backlog task edit 635 ...` to fix; never edit the file directly.
1. **[Create]** TASK-636 via `backlog task create`. Same procedure. Set dependencies to `TASK-635` via the CLI (if a `--dep` or `--depends-on` flag exists; otherwise document in the description).
1. **[Verify created]** `backlog task 636 --plain`.
1. **[Write]** `backlog/milestones/m-2 - v2.0.0.md` with the exact content from Artefact 3.
1. **[Write]** `backlog/milestones/m-3 - v2.1.0.md` with the exact content from Artefact 3.
1. **[Verify]** `ls backlog/milestones/` shows both files.
1. **[Commit]** Single commit with message referencing both task IDs:
   
   ```
   backlog: add TASK-635 (PV-surplus boost), TASK-636 (Modbus TCP), milestones v2.0.0 and v2.1.0
   
   TASK-635 targets v2.0.0; TASK-636 targets v2.1.0 (deferred).
   Milestones m-2 (v2.0.0) and m-3 (v2.1.0) added as release-markers
   alongside existing m-0 (Audit SAT) and m-1 (OTGW32 Support).
   ```
   
   The `.githooks/commit-msg` hook will validate that referenced TASK-NNN files exist in the index.
1. **[STOP]** Do NOT take ownership of TASK-635 (do not `-s "In Progress" -a @claude`). Do NOT start implementation. This handover is for backlog setup only. The user will assign TASK-635 to an implementation agent (possibly you, possibly a separate session) once they’ve reviewed the created tasks.

-----

## Known unknowns and risks for the agent

**Renumbering risk.** If task IDs 635/636 are taken between this handover (2026-05-20) and you starting work, the user should be asked. Do not silently bump to 637/638.

**ADR renumbering risk.** Same for ADR-104. If multiple PRs land in parallel, the next free number drifts.

**TASK-210 coordination.** TASK-635 AC #13 (OpenAPI for `/api/v2/sat/pvsurplus`) overlaps with TASK-210 (SAT REST endpoints to openapi.yaml). Run `backlog task 210 --plain` to check its status. If TASK-210 is still active, coordinate with that owner before implementing AC #13.

**Backlog CLI dependency flag.** I do not know whether `backlog task create` has a flag for declaring inter-task dependencies in this project. The CLI guide I saw did not explicitly document one. Check `backlog task create --help` and fall back to noting “Depends on TASK-635” in the description if no flag exists.

**Line-number drift in TASK-635 AC #6.** The acceptance criterion references lines 820, 3104, 3644 in `SATcontrol.ino` as of 2026-05-20. These will drift as the codebase evolves. The implementer must re-locate the call sites at implementation time by searching for direct reads of `settings.sat.fTargetTemp`. AC text says “as of 2026-05-20 — verify line numbers at implementation time” to flag this.

**Library evaluation spike for TASK-636.** This is a ½-day investment before any implementation can start. Budget accordingly when this task is picked up.

**Resource budget on ESP32 (AC #21 of TASK-636).** < 12KB RAM, < 50KB flash. Validate before merging, not after. If the chosen library blows the budget, escalate to user — do not silently exceed.

**SAT struct growth on ESP8266 (TASK-635 implementation notes).** Six new settings fields grow `SATSection` struct. Check flash footprint on ESP8266 Arduino Core 2.7.4 build before merging. If close to a limit, escalate.

-----

## What is NOT in this handover

- Implementation code for either task. Both need plan approval first per CLAUDE.md.
- Build, lint, or test execution. Reserved for post-implementation per pr-checklist.md.
- Branch creation, PR opening. Single commit on `feature-dev-2.0.0-otgw32-esp32-sat-support` is sufficient for backlog setup.
- Documentation files referenced in ACs (`docs/features/modbus-tcp-client.md`, ADRs, manuals). These are deliverables of the implementation, not of this backlog setup.

-----

## Source materials and rationale trail

Full analysis chat session (2026-05-20) covered:

- Feature comparison: OTGW-firmware v1.5 vs v2.0 branch vs Laxilef/OTGateway vs ESPHome OpenTherm vs Alexwijn/SAT
- Gap analysis identifying PV-surplus (gap 8) and Modbus (gap 3) as worth addressing
- Scope decisions: opt-in, default-off, ESP32-only for Modbus, safety bounds for PV boost
- Versioning decisions: TASK-635 in v2.0.0, TASK-636 deferred to v2.1.0
- Milestone structure: m-2 and m-3 as release-markers, complementary to existing m-0/m-1 audit milestones

The chat session is the canonical source. This handover document is a derived artefact. If the agent has access to the chat, prefer that for context on edge cases; if not, this document is self-contained for the backlog-creation step.