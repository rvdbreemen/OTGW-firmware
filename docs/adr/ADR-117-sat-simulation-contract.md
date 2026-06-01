---
id: ADR-117
title: SAT Simulation Contract — Bus Isolation, Boiler-Absence Availability, Command Trace
status: Proposed
date: 2026-06-01
tags: [sat, simulation, safety, opentherm, otgw32]
supersedes: []
superseded_by: []
related: [ADR-051, ADR-088]
deciders: [Robert van den Breemen]
---

# ADR-117: SAT Simulation Contract — Bus Isolation, Boiler-Absence Availability, Command Trace

## Status

Proposed. Guideline-level (per ADR-080): this ADR documents a runtime
contract and a code convention. It is enforced by code review plus the
self-checks below, not by a dedicated `evaluate.py` gate in this release. A
follow-up may promote the wrapper-usage convention to a CI `forbid_pattern`
once the existing direct-read sites that are intentionally exempt
(`RelModLevel`, the wrapper bodies themselves) can be excluded cleanly.

**Decision Maker:** User: Robert van den Breemen.

## Status History

status_history:
  - date: 2026-06-01
    status: Proposed
    changed_by: Claude (TASK-795)
    reason: Documents the SAT simulation contract implemented in commits 1-3 on the 2.0.0 line (wrappers, synthetic boiler model, bus-tx isolation, edge-triggered availability gate, command trace).
    changed_via: adr-kit

## Context

The SAT (Smart Autotune Thermostat) subsystem shipped an experimental
simulation mode (`settings.sat.bSimulation`, Task #37) that modelled only the
building thermal envelope: room temperature, a linear flow-temperature ramp,
and a constant outdoor temperature. It did **not** model anything boiler-side
(flame, modulation, return temperature), so large parts of the SAT algorithm —
the cycle classifier, flame-health state machine, 4-hour window statistics, the
daily heating-curve recommendation, and OPV calibration — never executed under
simulation because they read `OTcurrentSystemState.Tboiler` / `.Tret` / the
flame status bit / `RelModLevel`, all of which stay zero without a real boiler.

Two further problems existed:

1. **No isolation guarantee.** Nothing stopped a user from enabling simulation
   on a device with a real boiler attached. SAT would then regulate against
   synthetic state while still emitting OpenTherm commands the boiler obeys.
2. **No visibility.** A user enabling simulation could not see what the
   regulator *would* have commanded — the loop ran but its output went nowhere
   observable.

This branch drives the boiler-side OT bus through **two** distinct hardware
paths, both of which must be safe under simulation:

- **`HAS_PIC` builds** (ESP8266 + OTGW PIC, and ESP32-S3 + PIC variants) talk to
  the PIC over UART via `sendPICSerial()` in `OTGW-Core.ino`.
- **OTGW32** (ESP32-S3 native, `HAS_DIRECT_OT`) drives the bus directly via
  OTDirect; a single emitter, `sendMasterRequestAsync()` in `OTDirect.ino`,
  carries **both** gateway-origin commands and the thermostat-to-boiler
  pass-through/translation path. (An earlier design feared a separate
  translation emitter; the code review confirmed there is only one — one gate
  covers both origins.)

## Decision

Simulation operates under a three-invariant runtime contract.

### 1. Bus isolation (no boiler-side bytes leave the device)

A single shared gate helper, `satSimulationBlocksBusTx(cmd, source)`
(`SATcontrol.ino`, prototyped in `OTGW-firmware.h`), returns true — caller must
not emit — whenever `settings.sat.bSimulation` **or** the legacy
`state.debug.bOTGWSimulation` is active. It is invoked at the two lowest-level
boiler-side chokepoints:

- **Path (a)** `OTGW-Core.ino::sendPICSerial()` — the pre-existing
  `bOTGWSimulation` block now routes through the helper, so `bSimulation` also
  blocks PIC serial.
- **Paths (b)+(c)** `OTDirect.ino::sendMasterRequestAsync()` — the single shared
  master-TX emitter. Loopback mode returns earlier (its responses are
  synthesised and never reach the bus), so the gate sits on the real-bus branch.

Thermostat-side replies (OTGW32 acting as an OT slave to a wall thermostat) are
**not** gated: those bytes never reach the boiler. The cut is strictly
boiler-side.

### 2. Boiler-absence availability gate

Simulation is a boiler-absent bench mode, not a co-existence mode. The moment a
real boiler frame is observed, simulation is forcibly and completely disabled:

- An edge hook, `satNotifyBoilerFrameSeen()`, is called from the real-boiler-RX
  site of both transports (`OTGW-Core.ino` `buf[0]=='B'` PIC boiler frame;
  `OTDirect.ino::handleMasterResponse()` SUCCESS branch). It only raises
  `state.sat.bBoilerDetectedFlag` — cheap and interrupt-safe.
- `satControlLoop()` drains the flag in cooperative context via
  `satConsumeBoilerDetected()`, which runs `satOnBoilerDetected()`: flips
  `bSimulation` false, persists with `writeSettings(false)`, tears down synthetic
  state, clears the command trace, and emits a `'!'` WebSocket event. The MQTT
  state topic flips OFF on the next publisher tick. A defensive backstop in the
  same loop also calls `satOnBoilerDetected()` if a boiler is present while sim
  is somehow still set.
- `satBoilerHardwarePresent()` is the predicate for the API layers. It reads a
  **different** signal than the synthetic `boilerOnline`: on `HAS_PIC`,
  `state.otBus.bBoilerState` (set from real boiler frames, 30 s window); on
  `HAS_DIRECT_OT`, `otDirectBoilerPresent()` (boiler answered MsgID 3 —
  `otBoilerCacheValid[3]` — with loopback mode explicitly excluded). This
  dual-signal separation is the load-bearing safety property: the synthetic
  online state used by `probeOTBus()` must never feed the availability gate, or
  enabling simulation would immediately self-disable.
- Enable attempts while a boiler is present are refused: REST
  `PATCH /api/v2/sat/settings` with `simulation:true` returns **HTTP 409**;
  `updateSetting("SATsimulation", ...)` — the single chokepoint every writer
  (MQTT, settings restore) funnels through — rejects the enable, logs, and
  forces the flag false. The Web UI hides all `[data-sim-only]` elements when
  the status JSON reports `sim_available:false`.

### 3. Command trace

Every would-be boiler-side command captured at the gate is surfaced on three
read-only, outward-facing channels (this is presentation for external clients —
the firmware itself never round-trips through them):

- **telnet:** `SAT-SIM trace: <cmd> (src=<source>)`.
- **MQTT:** `sat/sim/last_cmd`, publish-on-change, **not retained**.
- **REST:** the SAT status JSON gains `last_blocked_cmd` + `last_blocked_cmd_age_ms`,
  plus `sim_available`, `sim_flame_on`, `sim_modulation`, `sim_return_temp`.

State added to `SATRuntimeSection` (`SATtypes.h`, all transient per ADR-051):
the synthetic-boiler fields (`bSimFlameOn`, `iSimModulation`, `fSimReturnTemp`,
`iSimFlameOnSinceMs`, `iSimFlameOffSinceMs`), the trace slot
(`sLastBlockedCmd[24]`, `iLastBlockedCmdMs`), and the edge-hook flag
(`bBoilerDetectedFlag`). No new persisted settings; all simulation tuning
constants are file-static `const` in `SATcontrol.ino`.

### Wrapper convention

SAT-internal reads of boiler-side OT state go through four wrappers —
`satGetFlowTemp()`, `satGetReturnTemp()`, `satIsFlameOn()`,
`satGetActualModulation()` — which return synthetic state when `bSimulation` is
set and the real `OTcurrentSystemState` field otherwise. New SAT code that needs
flow / return / flame / modulation MUST call the wrapper, not the raw field, so
it runs identically against a real boiler and the simulator.

## Alternatives Considered

- **Status quo (envelope-only sim).** Rejected: leaves the majority of SAT
  control paths untested under simulation, and offers no protection against
  enabling it with a real boiler attached.
- **Legacy `bOTGWSimulation` (serial replay) only.** Rejected: replays
  historical traffic and cannot exercise the loop's response to live changes in
  setpoint, weather, or PV surplus. Note this pre-existing flag never blocked
  the OTGW32 bus (its gate was inside `#if HAS_PIC`); this contract closes that
  gap as a side effect by OR-ing both flags at every chokepoint.
- **Per-emitter gating without a shared helper.** Rejected: duplicating the
  predicate and trace logic at each chokepoint guarantees drift; a future
  contributor adding an emitter would not know to copy it.
- **Single boiler-present signal reused for both the gate and `probeOTBus`
  synthetic-online.** Rejected: collapsing the two signals makes the gate
  self-disabling under simulation. The split is deliberate.

## Consequences

Positive:

- The full SAT algorithm — cycle classifier, flame health, 4 h stats,
  heating-curve recommendation, OPV calibration — runs on a hardware rig with no
  boiler attached.
- Field-deployed devices are unaffected: `bSimulation` defaults false, and the
  Web UI surface is hidden whenever a boiler is detected.
- Operators get a clear trace of what the regulator would have done.

Negative / risks:

- A new runtime invariant for contributors: every new boiler-side emit path must
  route through `satSimulationBlocksBusTx()`, and every new SAT boiler-state read
  should use the wrappers. Guideline-enforced for now (see Status).
- `writeSettings()` runs once at the moment of boiler detection (one LittleFS
  flush per detection event; the deferred-flag pattern guarantees once, not
  per-frame).
- `probeOTBus()` synthetic-online is **not** gated: it is a one-shot at
  `initOTDirect()` (setup, before the cooperative loop). A persisted-sim boot
  with a real boiler attached emits at most that single probe frame during the
  provisional window, after which the `satControlLoop` backstop disables sim on
  the next tick. Documented rather than gated to keep the dual-signal split
  intact.

## Self-checks (in lieu of a dedicated CI gate)

- `python build.py` green on both `HAS_PIC` (ESP8266) and `HAS_DIRECT_OT`
  (ESP32-S3) targets — the gate helper, edge hook, and presence predicate are
  capability-gated with `HAS_*` flags, never raw platform `#ifdef`s, so both
  compile.
- `python evaluate.py --quick` shows no new failures, including the
  design-system-drift check (the Web UI changes add no new CSS classes — they
  use the existing `sat-row`/`sat-label`/`sat-value` classes and a
  `data-sim-only` attribute).

## Related Decisions

- ADR-051 Settings/State architecture — all new simulation fields are transient
  `state.sat` members, honoured.
- ADR-088 MQTT status-burst windowing — the `sat/sim/last_cmd` publish is
  publish-on-change and non-retained, consistent with the SAT shadow pattern.
- ADR-080 Binding ADR rules must have a CI gate — this ADR is labelled
  guideline-level per that meta-rule.

## References

- TASK-795 SAT simulation contract (implementation).
- Plan: `docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md`.
- Implementation commits: `daf99b0f` (wrappers + migration), `4c33241b`
  (synthetic boiler model), `2445a0da` (bus-tx isolation + trace), plus this
  commit (availability gate + REST/MQTT/Web UI surfaces).
