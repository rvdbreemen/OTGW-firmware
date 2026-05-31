# SAT Simulation — Boiler model + isolation contract (Plan for review)

**Target branch:** `feature-dev-2.0.0-otgw32-esp32-sat-support`
**Scope sizing:** MVP boiler model (items 1 + 2 + 3 from discussion) **plus** simulation contract (bus isolation, boiler-absence availability gate, visible command trace).
**Origin:** Worktree absent in this remote container — plan executed by an agent spawned in the 2.0.0 worktree on a workstation that holds it.

---

## 1. Context

The current SAT simulator (`satUpdateSimulation()` in `src/OTGW-firmware/SATcontrol.ino:3072`) models the **building thermal envelope only**:

| Modelled today | Not modelled today |
|---|---|
| Room temperature (°C/min, configurable rates) | Flame on/off events |
| Sim flow temperature (°C/s, with 5-min warmup) | Modulation (MM) |
| Constant outdoor temperature | Return temperature (Tret) |
| | Cycle classification, 4h window stats |
| | Flame health, heating-curve recommendation |
| | OPV calibration state machine |

`satGetRoomTemp()` and `satGetOutsideTemp()` already proxy real sensors to the sim. `fSimFlowTemp` is computed but never injected — consumers still read `OTcurrentSystemState.Tboiler` and the OT flame bit, both of which stay zero without a real boiler. As a result, large sections of the SAT algorithm (cycle classifier, `eFlameStatus`, daily heating-curve recommendation, modulation-reliability, OPV calibration, return-flow Δ statistics) never execute under simulation.

This plan adds a **minimal but coherent boiler-side model** — synthetic flame, modulation, flow and return temperature — and routes SAT consumers through wrapper getters, so the same code paths that run against a real boiler run against the simulator. Expected coverage gain: ~40-60% of SAT-internal control paths previously unreachable.

Three runtime invariants apply whenever the simulator is active:

1. **No bus traffic.** No bytes physically affecting the heating system leave the device on any actuation path.
2. **Availability gate — boiler-absence required.** Simulation is only available when no boiler slave is detected on the OT bus. A standalone bench setup with neither a thermostat nor a boiler attached is the intended use case. The moment a boiler responds, simulation is forcibly disabled and the Web UI hides the control.
3. **Command trace.** Every command the SAT loop would have emitted is surfaced as a visible diagnostic (telnet, MQTT, REST) so the user can see *what* the regulator decided even though nothing left the device.

---

## 2. Goals (verifiable)

After this change, with `settings.sat.bSimulation = true` and **no** real boiler attached:

- G1. `satCycleOnFlameChange()` fires on synthetic flame edges; `state.sat.iCycleCount` increments.
- G2. `state.sat.eLastCycleClass` reaches `SAT_CYCLE_GOOD` / `SAT_CYCLE_OVERSHOOT` / `SAT_CYCLE_SHORT` depending on tuning.
- G3. `state.sat.iCyclesThisHour`, `f4hDutyRatio`, `f4hAvgOnSec`, `f4hAvgOffSec`, `f4hFlowRetDeltaP50/P90` populate from sim data after one observation window.
- G4. `state.sat.iCurrentModulation` varies between `min_mod` and `iMaxRelModulation` as the PID error changes.
- G5. `state.sat.eFlameStatus` advances out of `SAT_FS_INSUFFICIENT_DATA`.
- G6. OPV calibration (`satOvpCalibrate()`) completes its phase machine when triggered.
- G7. **Zero bytes** reach the boiler-side bus on any of the three actuation paths for the duration of any simulation session:
  - **(a) OTGW PIC serial** (any `HAS_PIC` build — ESP8266 + PIC board, ESP32-S3 Mini D1 + PIC, etc.) — `OTGW-Core.ino` serial-write chokepoint.
  - **(b) OTDirect master TX** (ESP32-S3 OTGW32, native OT master) — `OTDirect.ino` master dispatch + probe.
  - **(c) OTGW32 translation / pass-through layer** (ESP32-S3 OTGW32, thermostat-frame forwarding into the boiler-side bus, including OT-state-cache emit and any command-injection translation) — every boiler-side emit must be silent regardless of whether the trigger originated in SAT, in REST/MQTT/UI, or in pass-through.
  Verified by telnet debug, logic-analyser / oscilloscope tap on the boiler-side bus pins, and the `'!'`-level WebSocket event tagged `SAT-SIM`.
- G8. Every SAT command the loop would have sent (`CS=`, `MM=`, `CH=`, `TC=`, …) is visible in (a) the telnet debug stream tagged `SAT-SIM trace:`, (b) the MQTT topic `sat/sim/last_cmd` (single rolling value, retained=false), and (c) the SAT status JSON returned by REST under `last_blocked_cmd` plus an age-in-ms field.
- G9. When a boiler slave responds on the OT bus (`satBoilerHardwarePresent()` returns true), the simulator is **forcibly disabled** within one control loop, an `'!'`-level event names the cause, and the Web UI hides the simulation card on its next status refresh. A REST `PATCH` or MQTT command trying to set `bSimulation = true` while a boiler is present returns HTTP 409 (REST) or is rejected with a documented warning (MQTT).
- G10. A standalone bench rig with **no thermostat and no boiler** can enable `bSimulation` and run the full SAT loop end-to-end — i.e. boiler-absence is the only required pre-condition, thermostat-absence is permitted.

---

## 3. Non-goals

- Energy-accurate thermodynamics. The model is illustrative, not predictive.
- Dynamic outdoor temperature (sine, weather feed) — deferred to follow-up task.
- Sensor noise / dropouts — deferred.
- Multi-zone room model — deferred (single-zone room model only).
- DHW demand simulation — deferred.
- Scenario / event injection via REST — deferred.

---

## 4. Simulation contract

Three invariants — bus-transmit isolation (§4.1), availability gating (§4.2), command-trace visibility (§4.3) — are the simulator's contract with the rest of the firmware. All three are enforced at well-defined chokepoints so that no subsystem can accidentally violate them.

### 4.1 Bus-transmit gates (no bytes to the heating system)

The intent is **system-wide**: when `settings.sat.bSimulation` is true, the firmware behaves as if the heating hardware is not present. No bytes physically affecting the heating system may leave the device — regardless of what subsystem produced them (SAT loop, REST/MQTT/UI commands, OTGW pass-through, periodic write-cache refresh).

This branch has **three** distinct physical actuation paths into the boiler-side bus. All three must be gated at the lowest-level chokepoint, so the contract holds independently of which subsystem produced the byte:

| # | Path | Hardware | Chokepoint | Existing gate? |
|---|---|---|---|---|
| (a) | OTGW PIC serial | **Any** host MCU with `HAS_PIC` defined — ESP8266 + OTGW PIC board AND ESP32-S3 (incl. Mini D1) + PIC builds | `OTGW-Core.ino:3043` (`#if HAS_PIC`) | Yes — fires on `bOTGWSimulation` only |
| (b) | OTDirect master TX | ESP32-S3 OTGW32 (no PIC) | `OTDirect.ino:1207` — `otMaster.sendRequestAsync()` + `OTDirect.ino:841,867,877,896,906` `probeOTBus()` handshake | **No — currently unguarded** |
| (c) | OTGW32 translation / pass-through | ESP32-S3 OTGW32 | The boiler-side master-frame emitter inside the thermostat→boiler forwarding path (and any OT-state-cache emit, command-injection translation, periodic SAT command refresh) — implementing agent locates the actual function (likely a `forwardToBoiler()` / `emitMasterFrame()` / `otMaster.send*` shared chokepoint in `OTDirect.ino` or a sibling file) | **No — currently unguarded** |

Pre-existing gap: on OTGW32 hardware the legacy `bOTGWSimulation` flag never blocked OT-bus traffic because its gate was inside `#if HAS_PIC`. This plan closes that gap as a side effect — both `bOTGWSimulation` and `bSimulation` now disable paths (b) and (c). The single OR-clause idiom (`bOTGWSimulation || settings.sat.bSimulation`) is repeated at each chokepoint and emits the existing `'!'`-level WebSocket event with a source tag identifying which gate tripped (`SAT-SIM` vs `OTGW-SIM`).

**Where paths (b) and (c) converge in code, gate once at the converged site** — don't double-gate the same byte, since that risks double-counting in the §4.3 trace. Where they diverge (e.g. probe vs translation), gate each independently. The implementing agent's first task is to map (b) vs (c) precisely; if they share a common emitter, the table above collapses to two rows.

**Thermostat-side responses are NOT gated.** OTGW32 acting as an OT slave to a wall thermostat may continue to emit synthetic replies on the thermostat-side bus — those bytes never reach the boiler. The cut is strictly the boiler-side bus. On a bench rig with no thermostat, the thermostat-side bus is idle anyway.

Non-actuating side effects (telnet debug, MQTT publishes of synthetic state, WebSocket log fan-out) are explicitly **allowed** — they don't reach the heating system and are necessary for diagnostics. Likewise the write-command queue (`addCommandToQueue`, OT write cache at `OTDirect.ino:579-2030`) may continue to fill; only the bus-write at the bottom is blocked. This keeps the SAT control loop fully exercised — including queue coalescing (TASK-565) and write-cache refresh timing.

### 4.2 Availability gate: simulation requires boiler-absence

Simulation is a **diagnostic / bench-test mode**, not a co-existence mode. Enabling it on a device that has a real boiler attached would let the user reason about non-existent state while the actual boiler keeps running on stale OT commands. The gate prevents that. A standalone bench rig with no thermostat and no boiler is the intended use case — thermostat-absence is permitted; boiler-absence is required.

**Detection — edge-triggered, not periodic-polled.** The moment a boiler-originated slave frame arrives, simulation must stop. Latency target: ≤ one control-loop iteration (≤ 1 s). Implementation has two parts:

1. **Edge hook in the slave-frame parser** (likely `OTDirect.ino` slave-RX handler + `OTGW-Core.ino` `processOTGWmessage()` for the PIC path): on every received slave frame, if `settings.sat.bSimulation == true`, call `satOnBoilerDetected()` once. This is the primary detection — no 60 s window of grace, the first frame trips it.
2. **Predicate `satBoilerHardwarePresent()`** for the REST / MQTT / UI layers, which need to answer "is the boiler currently present?" without waiting for an edge:

```cpp
// Independent of OTcurrentSystemState.boilerOnline (the latter is overridden
// during simulation by §5.7's probeOTBus synthetic-online branch). Reading a
// different signal here prevents the synthetic-online from feeding back into
// the availability gate.
static bool satBoilerHardwarePresent() {
  // Sticky: once any slave MemberID has been observed this boot, the boiler
  // is considered present until reboot or sustained quiet. iSlaveMemberID
  // is cleared by the OTDirect slave-timeout path; we trust that signal.
  if (state.sat.iSlaveMemberID != 0) return true;
  // Recent slave frame within SAT_BOILER_PRESENCE_WINDOW_MS — covers the case
  // where MemberID has not yet been received but other slave traffic is on
  // the bus (e.g. between boot and first MsgID 3).
  if (otcLastSlaveInboundMs != 0 &&
      (millis() - otcLastSlaveInboundMs) < SAT_BOILER_PRESENCE_WINDOW_MS) return true;
  return false;
}
```

`SAT_BOILER_PRESENCE_WINDOW_MS = 60000` (1 min — confirmation window for the "currently absent" determination used by REST/UI; not used for the auto-disable, which is edge-triggered).

**`satOnBoilerDetected()` — complete teardown, one-way switch:**

```cpp
// Called from the slave-frame edge hook (paths a/b/c slave RX). Idempotent:
// safe to call on every frame even after sim is already off.
static void satOnBoilerDetected() {
  if (!settings.sat.bSimulation) return;  // already off — nothing to tear down

  // 1. Hard-disable in settings, persist immediately.
  settings.sat.bSimulation = false;
  writeSettings();  // LittleFS flush — survives reboot

  // 2. Tear down synthetic boiler state so the next loop iteration reads real bus.
  state.sat.bSimFlameOn          = false;
  state.sat.iSimModulation       = 0;
  state.sat.fSimFlowTemp         = OTcurrentSystemState.Tboiler;  // adopt real reading
  state.sat.fSimReturnTemp       = OTcurrentSystemState.Tret;
  state.sat.iSimFlameOnSinceMs   = 0;
  state.sat.iSimFlameOffSinceMs  = 0;
  state.sat.iSimLastUpdateMs     = 0;  // forces re-init if user later re-enables

  // 3. Clear the §4.3 command trace — its meaning ends with sim.
  state.sat.sLastBlockedCmd[0]   = '\0';
  state.sat.iLastBlockedCmdMs    = 0;

  // 4. Tell the world.
  OTDebugTln(F("SAT-SIM: boiler appeared on bus — simulation disabled completely"));
  sendEventToWebSocket_P('!', PSTR("SAT-SIM: boiler appeared, simulation off"));
  // MQTT state topic flips to OFF on the next sat-publisher tick (existing
  // s_simulation shadow detects the change automatically).
}
```

**One-way semantics.** Once `satOnBoilerDetected()` has fired, simulation stays off. Re-enabling via REST or MQTT while a boiler is still present is rejected (HTTP 409 / MQTT warning). Re-enabling becomes possible again only after either (a) a reboot with no boiler attached, or (b) the boiler slave times out and the OTDirect / OTGW-Core layer clears `iSlaveMemberID` and stops feeding `otcLastSlaveInboundMs`. The user does not have to manually flip anything — boiler-absence is the sole pre-condition.

**Three-layer enforcement** (defence in depth — the edge hook is the primary, the layers below catch anything that misses the edge):

| Layer | Behaviour when boiler present | Behaviour when boiler absent |
|---|---|---|
| **Edge hook** (primary) | First slave frame triggers `satOnBoilerDetected()` — full teardown within one control loop. | — (no frames to trigger on). |
| **Runtime** (backstop) | `satEffectiveSimulationActive()` returns false even if `settings.sat.bSimulation == true`. Boiler model, getters, transmit gates all behave as if simulation is off. A periodic check in the SAT loop also calls `satOnBoilerDetected()` defensively, in case the edge hook was missed (e.g. a frame type not yet routed through the hook). | Simulation runs as designed. |
| **REST** | `PATCH /api/v2/sat/settings` carrying `bSimulation: true` returns HTTP 409 with `{"error":"simulation unavailable: boiler hardware detected"}`. Documented in `restAPI.ino`. | Accepted normally. |
| **MQTT** | Inbound `sat/simulation/set` payload `ON` is rejected with a telnet warning + the state topic re-publishes `OFF`. | Accepted normally. |

**Web UI surface** (`data/index.html` + `data/index.js`):

- REST status JSON gains `sim_available` (bool — mirrors `!satBoilerHardwarePresent()`).
- The simulation card and the diagnostic block on the SAT page are wrapped in a container hidden by default. `index.js`'s SAT status-refresh handler toggles `.hidden` based on `sim_available`. When the card is hidden, the corresponding MQTT discovery sensors stay published (their values just freeze) — discovery churn on every boiler hotplug would be worse than leaving them visible-but-stale in HA.
- A small explanatory tooltip ("Simulation is only available on a bench setup with no boiler attached") explains the disappearance.

**Boot behaviour**: at boot we may not yet know whether a boiler is present. The plan: leave the user-saved `bSimulation` value as is, but consider it **provisional** until the first probe completes. During this provisional window (max `SAT_BOILER_PRESENCE_WINDOW_MS`), the boiler model and bus-tx gates already behave correctly (no real bus traffic anyway because boiler not yet probed). Once a slave is detected, the auto-disable fires; if no slave detected within the window, simulation remains enabled.

### 4.3 Command trace: visible would-be commands

The user must be able to see *what* the SAT regulator decided, even though nothing physical happened. Three surfaces, one capture point.

**Capture point**: the bus-transmit gate helper from §4.1 receives the would-be command as a short ASCII string (e.g. `"CS=42.5"`, `"MM=80"`, `"TC=21.0"`). Each gate site formats its command to ASCII before calling the helper:

- `OTGW-Core.ino` PIC path: the cmd buffer is already ASCII — pass directly.
- `OTDirect.ino`: format the OT frame as `"MID=24 VAL=4200"` (msgID + uint16 value) before the helper call.

**State** (one slot — last-write-wins, no ring buffer in MVP):

```cpp
// Last would-be command captured by the bus-tx gate (only when bSimulation is
// true — bOTGWSimulation keeps its existing diagnostic surface unchanged).
char     sLastBlockedCmd[24]      = {0};
uint32_t iLastBlockedCmdMs        = 0;
```

(~30 bytes — added to the §6 state-fields budget.)

**Surfaces** (all driven from the same captured string):

| Surface | Format | Notes |
|---|---|---|
| Telnet debug | `SAT-SIM trace: CS=42.5 (src=loop)` | One line per blocked command; respects existing `OTDebugTf` rate-limit conventions. |
| MQTT | Topic `sat/sim/last_cmd`, payload `"CS=42.5"`, **not retained**. | Published from `MQTTstuff.ino`'s sat publisher when `sLastBlockedCmd` changed since last publish. Adds one entry to the existing `s_*` shadow-state pattern. |
| REST status JSON | `"last_blocked_cmd":"CS=42.5", "last_blocked_cmd_age_ms":1234` | Inside the existing `sat/simulation` JSON block at `restAPI.ino:1896`. Omitted (or empty string + age 0) when sim is inactive. |

**MQTT discovery**: one new sensor (`<host>-sat_last_blocked_cmd`) wired through the existing `MQTTHaDiscovery.cpp` SAT block. Visible in HA only while the SAT simulation card is visible in the Web UI (i.e. while no boiler is attached) — see §4.2 for why we don't suppress discovery on hot-plug.

A ring of the last N blocked commands is explicitly **out of scope** for MVP — captured as follow-up F6.

---

## 5. Design

### 5.1 Wrapper getters (new, mirror existing pattern)

Add four wrappers in `SATcontrol.ino`, alongside the existing `satGetRoomTemp()` / `satGetOutsideTemp()` (regs. 969 / 1045):

| Wrapper | Sim source | Real source |
|---|---|---|
| `satGetFlowTemp()` | `state.sat.fSimFlowTemp` | `OTcurrentSystemState.Tboiler` |
| `satGetReturnTemp()` | `state.sat.fSimReturnTemp` | `OTcurrentSystemState.Tret` |
| `satIsFlameOn()` | `state.sat.bSimFlameOn` | `(OTcurrentSystemState.Statusflags & 0x08) != 0` |
| `satGetActualModulation()` | `state.sat.iSimModulation` | OT MsgID 17 (`Slave.RelativeModulationLevel`) |

Each returns the sim value when `settings.sat.bSimulation` is set, otherwise the real bus state.

### 5.2 Synthetic flame state machine

Inside `satUpdateSimulation()`, before the existing flow/room update:

```
heatRequested  = state.sat.bActive && state.sat.fFinalSetpoint > SAT_MIN_SETPOINT + 1.0f
flowBelowSP    = state.sat.fSimFlowTemp < (state.sat.fFinalSetpoint - SAT_SIM_FLAME_HYST_LO)
flowAboveSP    = state.sat.fSimFlowTemp >= (state.sat.fFinalSetpoint + settings.sat.fOvershootMargin)
offLongEnough  = (now - state.sat.iSimFlameOffSinceMs) >= SAT_SIM_MIN_OFF_MS  // 60 s

if !bSimFlameOn:
  if heatRequested && flowBelowSP && offLongEnough:
    bSimFlameOn = true;  iSimFlameOnSinceMs = now;  satCycleOnFlameChange(true)
else:
  if !heatRequested || flowAboveSP:
    bSimFlameOn = false; iSimFlameOffSinceMs = now; satCycleOnFlameChange(false)
```

Constants in the same file as the existing `SAT_SIM_*` block (regs. 64-66):

```
SAT_SIM_FLAME_HYST_LO     = 0.5f   // °C below setpoint to (re)ignite
SAT_SIM_MIN_OFF_MS        = 60000  // anti-cycle floor
SAT_SIM_MOD_KP            = 20.0f  // mod% per °C of error
```

No new `settings.sat` field needed — overshoot/setpoint reuse existing settings.

### 5.3 Synthetic modulation model

```
if bSimFlameOn:
  err   = fFinalSetpoint - fSimFlowTemp
  mod   = clamp(SAT_SIM_MOD_KP * err, minMod(), settings.sat.iMaxRelModulation)
  iSimModulation = (uint8_t)mod
else:
  iSimModulation = 0
```

`minMod()` returns 10 when `satGetManufacturerQuirks() & SAT_QUIRK_MIN_MOD_10` (Geminox), else 5. Reuses existing quirk infrastructure.

### 5.4 Flow temperature model (refined)

Replace the linear rate model with one coupled to modulation:

```
power_in  = (iSimModulation / 100.0f) * settings.sat.fBoilerCapacity  // kW thermal
heat_loss = SAT_SIM_FLOW_LOSS_K * (fSimFlowTemp - fSimRoomTemp)       // simple linear
dT_per_s  = (power_in - heat_loss) * SAT_SIM_FLOW_THERMAL_GAIN        // °C/s
fSimFlowTemp += dT_per_s * dtSec
```

Tunable constants only (no new settings). When `!bSimFlameOn`, `power_in = 0` so flow naturally decays toward room temp — same end-state as today, but with realistic ramp shape that reflects modulation.

### 5.5 Synthetic return temperature

```
delta_TR = 5.0f + 15.0f * (iSimModulation / 100.0f)       // °C
fSimReturnTemp = max(fSimFlowTemp - delta_TR, fSimRoomTemp)
```

Sufficient to populate `f4hFlowRetDeltaP50/P90` and the cycle-tracker's `avgFlowRetDelta`.

### 5.6 Room temperature model (no change in shape)

Keeps existing `fSimHeatRate` / `fSimCoolRate` semantics. Heating-decision predicate switches from "PID active + setpoint above floor" to **`satIsFlameOn()`** — so the room only warms when the synthetic boiler is actually firing. More faithful, no new constant.

### 5.7 Bus-transmit gate helper (implementation of §4.1 + §4.3)

Gate sites and rationale are described in §4.1 (the contract). The implementation is one small inline helper that does both jobs — blocks the write **and** captures the would-be command for the §4.3 trace:

```cpp
// cmd:    short ASCII representation of what would have gone out (e.g. "CS=42.5"
//         or "MID=24 VAL=4200"). Caller formats; helper does not allocate.
// source: const PROGMEM literal naming the call site ("loop", "rest", "probe", ...).
static inline bool satSimulationBlocksBusTx(const char* cmd,
                                            const __FlashStringHelper* source) {
  if (!(state.debug.bOTGWSimulation || settings.sat.bSimulation)) return false;
  const char* tag = settings.sat.bSimulation ? "SAT-SIM" : "OTGW-SIM";
  OTDebugTf(PSTR("%s trace: %s (src=%S)\r\n"), tag, cmd, source);
  sendEventToWebSocket_P('!', PSTR("simulation blocked bus tx"));
  if (settings.sat.bSimulation) {
    strlcpy(state.sat.sLastBlockedCmd, cmd, sizeof(state.sat.sLastBlockedCmd));
    state.sat.iLastBlockedCmdMs = millis();
  }
  return true;
}
```

Each gate site formats the cmd string before calling the helper. OTGW-Core's PIC path already has ASCII on hand; OTDirect formats `"MID=<id> VAL=<u16>"` from the OT frame before dispatch and `"PROBE"` at probe entry.

`probeOTBus()` synthetic-online (returning `true` instead of `false` when sim is active) is implemented as a separate helper or a flag returned from `satSimulationBlocksBusTx`. The reason it can't simply leak through the same return code: §4.2's availability gate reads a **different** signal (last slave inbound), so the synthetic-online does not loop back into auto-disabling sim. The dual-signal split is critical — see §4.2's detection-predicate note.

---

## 6. New `state.sat` fields (SATtypes.h, SATRuntimeSection)

Append to the existing `// Simulation (Task #37)` block (around `SATtypes.h:fSimRoomTemp`):

```cpp
// Synthetic boiler state (sim items 1+2)
bool     bSimFlameOn          = false;
uint8_t  iSimModulation       = 0;       // 0-100 %
float    fSimReturnTemp       = 20.0f;   // °C
uint32_t iSimFlameOnSinceMs   = 0;
uint32_t iSimFlameOffSinceMs  = 0;
// Command trace (§4.3): last would-be command captured by bus-tx gate
char     sLastBlockedCmd[24]  = {0};
uint32_t iLastBlockedCmdMs    = 0;
```

All transient (state, not settings) per ADR-051. ~50 bytes additional BSS — negligible.

No additions to `SATSection` (settings) — all tuning constants are static `const` in `SATcontrol.ino` for the MVP. If beta feedback wants them tunable, a follow-up task can promote them.

---

## 7. Call-site migration (~17 sites)

Already inventoried via grep on the 2.0.0 tree (commit `9519087`):

| Pattern | File | Lines (count) |
|---|---|---|
| `(Statusflags & 0x08) != 0` → `satIsFlameOn()` | `SATcontrol.ino` | 334, 482, 668, 939, 2416, 2600, 2964, 3156, 3627, 3979 (10 sites) |
| `OTcurrentSystemState.Tboiler` → `satGetFlowTemp()` | `SATcontrol.ino` | 333, 484, 669, 768, 944, 2377, 2532, 2632, 3823 (9 sites) |
| `OTcurrentSystemState.Tboiler` → `satGetFlowTemp()` | `SATcycles.ino` | 551, 552, 562, 573, 649, 723 (6 sites) |
| `OTcurrentSystemState.Tret` → `satGetReturnTemp()` | `SATcontrol.ino` | 755, 2633 (2 sites) |
| `OTcurrentSystemState.Tret` → `satGetReturnTemp()` | `SATcycles.ino` | 656 (1 site) |

**Out of scope for migration**: non-SAT consumers (MQTT publishers in `MQTTstuff.ino`, REST API, OLED, WebSocket). They keep reading the OT bus directly. Under simulation, those readings will be stale/zero — that is acceptable and matches how `bOTGWSimulation` already behaves. Users who want sim values in MQTT continue to use the existing `sat/simulation` topic and the `sim_*` JSON fields (already published).

---

## 8. Critical files

| File | Change type | Approx. LOC |
|---|---|---|
| `src/OTGW-firmware/SATtypes.h` | Add 7 fields to SATRuntimeSection (flame, mod, Tret, on/off ms, trace cmd, trace ms) | +12 |
| `src/OTGW-firmware/SATcontrol.ino` | 4 wrappers + flame-SM + mod model + Tret + migrate 19 sites + `satUpdateSimulation` extension + `satBoilerHardwarePresent()` + `satEffectiveSimulationActive()` + periodic auto-disable check + `satSimulationBlocksBusTx()` helper | +250 / −30 |
| `src/OTGW-firmware/SATcycles.ino` | Migrate 7 call sites | +0 / ~7 lines edited |
| `src/OTGW-firmware/OTGW-Core.ino` | Extend serial-transmit gate via shared helper; format cmd buffer for trace; add slave-RX edge hook calling `satOnBoilerDetected()` | +14 / −2 |
| `src/OTGW-firmware/OTDirect.ino` | Gate path (b): `sendMasterRequestAsync()` (1 site) + `probeOTBus()` synthetic-online branch + format `"MID=… VAL=…"` for trace. Gate path (c): every boiler-side master-frame emitter in the translation / pass-through layer (implementing agent maps; collapses to (b) if they share a common emitter, otherwise +1 chokepoint). Add slave-RX edge hook calling `satOnBoilerDetected()`. | +30 |
| `src/OTGW-firmware/restAPI.ino` | Add `sim_flame_on`, `sim_modulation`, `sim_return_temp`, `sim_available`, `last_blocked_cmd`, `last_blocked_cmd_age_ms` to status JSON; HTTP 409 on `PATCH` enabling sim when boiler present | +35 |
| `src/OTGW-firmware/MQTTstuff.ino` | Reject inbound `sat/simulation/set ON` when boiler present; publish `sat/sim/last_cmd` on change | +25 |
| `src/OTGW-firmware/MQTTHaDiscovery.cpp` | One new discovery entry: `sat_last_blocked_cmd` sensor | +12 |
| `src/OTGW-firmware/data/index.html` | Wrap simulation card / SAT diagnostic block in `<div data-sim-only hidden>`, add tooltip | +6 / -2 |
| `src/OTGW-firmware/data/index.js` | Read `sim_available` from SAT status; toggle `.hidden` on `[data-sim-only]`; render `last_blocked_cmd` + age | +25 |

Total: ~390 LOC net addition. Single translation unit on the firmware side; LittleFS data updates as usual via filesystem image.

---

## 9. ADR considerations

**One new ADR is likely warranted; two more are borderline.** Sergeant's call.

### Likely required

- **ADR — "SAT simulation contract: bus isolation, boiler-absence availability, command-trace visibility"** (Proposed → Accepted before merge). Justification: §4 introduces a tri-invariant runtime contract that constrains future work (a new bus-tx path added later must respect §4.1; a new control-source UI must respect §4.2; a new control-emit path must respect §4.3). That's exactly the durable kind of decision ADRs exist for. The ADR would also formalise the dual-signal separation (synthetic-online vs slave-frame arrival) so a future contributor can't accidentally collapse it.

### Borderline

- **MQTT discovery surface** (`sat_last_blocked_cmd`) — one new sensor inside an existing block, not a new topic-shape policy. Probably covered by the simulation-contract ADR; no separate ADR.
- **REST 409 semantics** (`PATCH /api/v2/sat/settings` rejects sim-enable when boiler present) — a new error code for an existing endpoint. Documented inline in the simulation-contract ADR's Consequences section.

### Not needed

- ADR-051 (state/settings split) is honoured: all new fields are transient state.
- The MVP introduces no new ArduinoJson usage, no new String allocations in hot paths, no PROGMEM domain crossings, no HTTPS — none of the standing architectural ADRs are touched.

**Enforcement block on the new ADR**: candidates include `forbid_pattern` for direct `OTcurrentSystemState.Tboiler` reads in SAT files (steering future work to the wrappers) and `require_pattern` for the simulation-source tag in any new bus-tx site. Both heuristic but useful.

---

## 10. Verification plan

### Build / static gates (per CLAUDE.md push policy)

1. `python build.py --firmware` exits 0.
2. `python evaluate.py --quick` shows no new PROGMEM / unsafe-pattern findings.

### Behavioural — bench rig (no thermostat, no boiler)

3. **Bus-tx isolation (G7) — all three physical paths**:
   - **(a) ESP8266 + PIC build**: enable `bSimulation` via REST. Confirm `OTGW-Core.ino` gate fires for any SAT-issued `addCommandToQueue` and for any user-issued REST/MQTT command. Verify no bytes on Serial1 with a logic analyser or `Serial1` debug tap for one full minute.
   - **(b) ESP32-S3 OTGW32 — OTDirect master TX**: enable `bSimulation` via REST. Confirm `OTDirect.ino` gate at `sendMasterRequestAsync` blocks every periodic write (15 s `OT_WRITE_INTERVAL_MS`) and that `probeOTBus()` returns synthetic-online without driving the bus pins.
   - **(c) ESP32-S3 OTGW32 — translation / pass-through layer**: with `bSimulation` enabled, attach a wall thermostat (no boiler). Trigger the thermostat to send a master frame (any control command). Confirm via oscilloscope on the boiler-side bus pins and the OT-direct bridge log (TCP 25238) that **no boiler-side frame is emitted** — translation/forwarding is fully blocked. Thermostat-side replies from OTGW32 are permitted and observable on the thermostat-side bus only.
   - In all three cases the `'!'`-level WebSocket event must appear, tagged `SAT-SIM`. The §4.3 command trace must capture the would-be boiler-side frames for paths (b) and (c).
4. **Flame events (G1, G2, G5)**: drop room sim target below `fSimRoomTemp`, set target above. Observe `state.sat.iCycleCount` increment by ≥1 within 5 minutes; `eLastCycleClass` transitions out of `SAT_CYCLE_NONE`; `eFlameStatus` advances out of `SAT_FS_INSUFFICIENT_DATA`.
5. **Modulation (G4)**: with a 2°C step error, confirm `iSimModulation` saturates near `iMaxRelModulation`. Reduce error to 0.2°C, confirm `iSimModulation` falls to `minMod()` floor.
6. **Return-flow delta (G3)**: after one rolling window, confirm `f4hFlowRetDeltaP50` is non-zero and < `f4hFlowRetDeltaP90` (sanity).
7. **OPV calibration (G6)**: trigger via existing REST endpoint, confirm `eCalibPhase` reaches `SAT_CALIB_DONE` rather than `SAT_CALIB_FAILED`.
8. **Command trace (G8)** — three surfaces, one capture:
   - Telnet: one `SAT-SIM trace: …` line appears per blocked command, format `<cmd> (src=<source>)`.
   - MQTT: `mosquitto_sub -t '<host>/sat/sim/last_cmd'` shows each new command within 1 s of the corresponding telnet line.
   - REST: `curl /api/v2/sat/status | jq '.simulation.last_blocked_cmd'` matches the latest telnet entry; `last_blocked_cmd_age_ms` is < 5 000 after a recent blocked write.

### Behavioural — availability gate (boiler attached)

9. **Availability gate, four layers (G9)** — connect a real boiler (or boiler emulator) to the bus, then:
   - **Edge-triggered auto-disable** (primary): pre-enable `bSimulation` while disconnected, attach boiler. Within **≤ 1 s** of the first slave frame arriving (one control-loop tick), observe:
     - (a) telnet `SAT-SIM: boiler appeared on bus — simulation disabled completely`,
     - (b) `bSimulation` flipped to false in `/api/v2/sat/settings`, persisted across reboot,
     - (c) synthetic state torn down — `state.sat.bSimFlameOn == false`, `iSimModulation == 0`, `sLastBlockedCmd == ""`,
     - (d) MQTT state topic `sat/simulation` flips to `OFF` within the next publisher tick.
     Verify on **both** ESP8266 PIC build and ESP32 OTGW32 build (the edge hook must fire on slave RX in `OTGW-Core.ino` AND in `OTDirect.ino` — testing one variant is not sufficient).
   - **Runtime backstop**: re-enable `bSimulation` via direct settings file edit (bypassing the REST gate) with boiler still attached; reboot. Within `SAT_BOILER_PRESENCE_WINDOW_MS` of boot, the periodic safety-net check fires `satOnBoilerDetected()` and disables again.
   - **REST 409**: with boiler attached, `curl -X PATCH /api/v2/sat/settings -d '{"bSimulation":true}'` returns HTTP 409 with the documented error body.
   - **MQTT reject**: publish `<host>/sat/simulation/set` payload `ON`; telnet shows the warning; state topic does not flip to `ON`.
   - **Web UI hide**: load the SAT page; the simulation card and diagnostic block carry the `hidden` attribute (CSS visibility = none); `data-sim-only` containers all hidden.
10. **Bench rig (G10)**: with neither thermostat nor boiler attached, `bSimulation` can be enabled and the full SAT loop runs (steps 4-8 all pass). Thermostat-absence does not block sim, only boiler-presence does.

### Regression — real boiler with `bSimulation = false`

11. **Real-boiler regression**: with `bSimulation = false` and a real boiler attached, confirm SAT command stream to PIC is byte-identical to a pre-change build for one 10-minute window (snapshot OT log and diff). This catches any wrapper-getter migration that accidentally changed runtime behaviour.

Field-validation gates 9-11 are the only ones not fully self-verifiable in the remote sandbox — they require the maintainer's beta rig and an attached boiler (or boiler emulator).

---

## 11. Backlog task

Single task on the **2.0.0 worktree** (use `npx -y backlog.md task create` if global `backlog` is missing in the agent's environment):

- **Title**: `feat-2.0.0: SAT simulation contract — boiler model, bus isolation, availability gate, command trace`
- **Description**: condensed §1-2 of this plan.
- **Labels**: `sat`, `simulation`, `safety`
- **Acceptance criteria** (mirror §10 G1-G10):
  1. `satIsFlameOn()` / `satGetFlowTemp()` / `satGetReturnTemp()` / `satGetActualModulation()` wrappers exist and route via `settings.sat.bSimulation`.
  2. All 19 call sites in §7 migrated to wrappers.
  3. Synthetic flame edges drive `satCycleOnFlameChange()` and increment `iCycleCount`; `eLastCycleClass` reaches a non-`NONE` value.
  4. `iSimModulation` varies between `minMod()` and `iMaxRelModulation` under varying PID error.
  5. `fSimReturnTemp = fSimFlowTemp − delta(mod)`, floored at `fSimRoomTemp`.
  6. **All three** boiler-side actuation paths are gated when `bSimulation` or `bOTGWSimulation` is true: (a) `OTGW-Core.ino` blocks PIC writes, (b) `OTDirect.ino` blocks `sendMasterRequestAsync()` + `probeOTBus()` (probe returns synthetic-online so SAT does not flip into fallback), (c) the OTGW32 translation / pass-through layer blocks every boiler-side forwarded frame. Thermostat-side replies are permitted. Verified on both ESP8266 PIC and ESP32 OTGW32 builds.
  7. Command trace: every blocked SAT command appears (a) on telnet tagged `SAT-SIM trace:`, (b) on MQTT topic `sat/sim/last_cmd` (non-retained), (c) in REST status JSON as `last_blocked_cmd` + `last_blocked_cmd_age_ms`. Trace covers paths (a), (b), and (c) — including translated thermostat-pass-through frames that would have reached the boiler.
  8. **Edge-triggered auto-disable** — when a boiler attaches with sim enabled, `satOnBoilerDetected()` fires within ≤ 1 s of the first slave frame, performs the full §4.2 teardown (settings flipped + persisted, synthetic state reset, command trace cleared, MQTT state flipped to OFF, telnet event emitted), and survives reboot. Verified on both ESP8266 PIC and ESP32 OTGW32 builds.
  9. Availability gate — REST: `PATCH /api/v2/sat/settings` setting `bSimulation:true` while boiler present returns HTTP 409 with the documented error body.
  10. Availability gate — MQTT: inbound `sat/simulation/set ON` while boiler present is rejected with a telnet warning and does not flip the state topic.
  11. Availability gate — Web UI: simulation card and diagnostic block are hidden when `sim_available=false`; visible when true.
  12. Standalone bench (no thermostat, no boiler): full SAT loop runs end-to-end; AC 3-5, 7 all observable.
  13. `python build.py --firmware` exits 0 for both ESP8266 and ESP32-S3 targets; `python evaluate.py --quick` shows no new findings.
  14. Real-boiler regression (`bSimulation = false` with boiler attached): SAT OT command stream byte-identical to pre-change build over a 10-minute window.
  15. New ADR ("SAT simulation contract") authored and Accepted before merge; Enforcement block included.

- **DoD** inherits project defaults; ACs 8-12, 14 are field-validation gates (require beta rig + boiler-or-emulator).
- **Push policy**: `origin/feature-dev-2.0.0-otgw32-esp32-sat-support` push allowed once AC 13 passes (per CLAUDE.md standing permission), with field-validation ACs left at `In Progress` if maintainer beta validation pending.

---

## 12. Out-of-scope follow-ups (capture as separate backlog tasks)

- **F1**: Diurnal `fSimOutdoorTemp` model (sine, configurable amplitude/phase, or pulled from `state.sat.weather.fTemperature` when valid).
- **F2**: REST scenario-injection endpoint `/api/v2/sat/sim/event` for `window_open`, `solar_gain`, `dhw_demand`, `pressure_drop`, `pv_surplus`.
- **F3**: Multi-zone / multi-area room model for `iZoneCount > 1` and `bMultiArea = true`.
- **F4**: Configurable sensor noise + occasional dropouts to exercise stale-detection and EMA filters.
- **F5**: DHW demand simulation (flame steal, CH-vs-DHW cycle fraction).
- **F6**: Command-trace ring buffer (last 10-16 blocked commands instead of single-slot). Useful for short-burst diagnostic scenarios; deferred because the single-slot surface is sufficient to verify the contract.
- **F7**: Boiler-emulator script (host-side TCP→OT bridge that replies to MsgID 3 + status polls) so AC 8-10 can be validated without a physical boiler on the bench.

---

# EXECUTOR HANDBOOK

The sections above are the design contract — sergeant-reviewed. The sections below are the cookbook a fresh Claude Code session uses to **execute** the plan in the 2.0.0 worktree. Self-contained: no other plan, doc, or chat history needed.

## 13. Pre-flight + reading order

### 13.1 Confirm you're in the right place

Run these checks before opening any source file. Every command should succeed; if any fails, stop and surface the issue.

```bash
# 1. Right branch?
git rev-parse --abbrev-ref HEAD
# Expected: feature-dev-2.0.0-otgw32-esp32-sat-support

# 2. Clean tree?
git status --porcelain | head -5
# Expected: empty (no uncommitted changes)

# 3. Up to date with origin?
git fetch origin feature-dev-2.0.0-otgw32-esp32-sat-support
git status -sb | head -3
# Expected: "## feature-dev-2.0.0-otgw32-esp32-sat-support...origin/... [...]"
#           with no "behind" count, or fast-forward if behind

# 4. Build baseline works (catches Arduino-CLI / toolchain drift early)
python build.py --firmware
# Expected: exit 0

# 5. Evaluator baseline clean
python evaluate.py --quick
# Expected: no new findings beyond pre-existing
```

If pre-flight passes: start the task (§16.1) and proceed to §13.2.
If any step fails: do NOT begin coding; report and wait.

### 13.2 Read these files before touching anything (in this order)

Pure-read pass to ground yourself. Don't edit, don't paste, just absorb structure.

| Order | File | What you're looking for |
|---|---|---|
| 1 | `CLAUDE.md` | The 2.0.0 worktree's local CLAUDE.md may override the dev one — read in full. |
| 2 | `src/OTGW-firmware/SATtypes.h` | `SATRuntimeSection`, `SATSection`, existing `// Simulation (Task #37)` block. |
| 3 | `src/OTGW-firmware/SATcontrol.ino` lines 60-70 (constants), 950-1100 (existing getters), 3070-3150 (`satUpdateSimulation`), 3600-3700 (SAT main loop entry) | Existing simulation surface and wrapper pattern. |
| 4 | `src/OTGW-firmware/SATcycles.ino` lines 540-740 | Cycle tracker — the consumer of `satGetFlowTemp` / `satGetReturnTemp` / `satIsFlameOn`. |
| 5 | `src/OTGW-firmware/OTGW-Core.ino` lines 3030-3070 (PIC serial-write gate area) + `grep -n "bOTGWSimulation" OTGW-Core.ino` | The existing OTGW-Sim gate — pattern to extend. |
| 6 | `src/OTGW-firmware/OTDirect.ino` lines 800-1250 + `grep -n "sendRequestAsync\|otMaster\.send\|probeOTBus" OTDirect.ino` | Path (b) chokepoints and the (c) translation layer. **Critical**: spend time here — (c) localisation is the riskiest mapping step. |
| 7 | `src/OTGW-firmware/restAPI.ino` lines 1850-1950 (existing SAT/simulation JSON block) + the SAT settings PATCH handler (grep for `kV2Routes\|sat/settings\|bSimulation`) | Where to hang `sim_available`, `last_blocked_cmd`, and HTTP 409. |
| 8 | `src/OTGW-firmware/MQTTstuff.ino` (search `s_simulation`, `sat/simulation`) | Existing shadow pattern to extend for `last_blocked_cmd`; inbound `set` handler to add 409-equivalent. |
| 9 | `src/OTGW-firmware/data/index.html` + `data/index.js` (search `simulation`, `data-` attribute conventions) | Where the SAT page lives and how it refreshes. |
| 10 | `docs/adr/` index | Find the next free ADR number; review one recent ADR's structure. |

After this pass you should be able to answer: where exactly does path (c) emit on the bus? where does the SAT settings PATCH handler validate inputs? where does the SAT status JSON render in HTML? If you can't, re-read.

---

## 14. Implementation playbook — three-commit split

Keep the diff reviewable. Each commit is independently building, evaluator-green, and self-meaningful. Don't merge them — separate commits, single PR.

### 14.1 Commit 1: Wrappers + call-site migration (low-risk, mechanical)

**Scope**: introduce the four wrappers, migrate the 19 call sites in §7. No behavioural change yet — wrappers return real bus state because the sim code paths to feed `fSimFlowTemp` / `fSimReturnTemp` / `bSimFlameOn` / `iSimModulation` don't exist yet (they default to safe values that match real bus state on a non-sim run).

**Code — append to `SATcontrol.ino` after `satGetOutsideTemp()` (around line 1042)**:

```cpp
//=====================================================================
//=== SAT simulation wrappers (ADR-NNN: simulation contract) ===
//=====================================================================
static float satGetFlowTemp() {
  if (settings.sat.bSimulation) return state.sat.fSimFlowTemp;
  return OTcurrentSystemState.Tboiler;
}

static float satGetReturnTemp() {
  if (settings.sat.bSimulation) return state.sat.fSimReturnTemp;
  return OTcurrentSystemState.Tret;
}

static bool satIsFlameOn() {
  if (settings.sat.bSimulation) return state.sat.bSimFlameOn;
  return (OTcurrentSystemState.Statusflags & 0x08) != 0;
}

static uint8_t satGetActualModulation() {
  if (settings.sat.bSimulation) return state.sat.iSimModulation;
  // OT MsgID 17 RelativeModulationLevel — confirm exact field name in
  // OTcurrentSystemState during reading-pass §13.2 step 5.
  return (uint8_t)OTcurrentSystemState.RelativeModulationLevel;
}
```

**Call-site migration** — mechanical. Use the grep table in §7 as a checklist; for each line, the change is the single-token swap (read-only access). Verify with:

```bash
# After editing, no direct reads remain in SAT files:
grep -nE "OTcurrentSystemState\.(Tboiler|Tret)" src/OTGW-firmware/SATcontrol.ino src/OTGW-firmware/SATcycles.ino
# Expected: only writes (assignments) remain, if any — no reads.

grep -nE "Statusflags *& *0x08" src/OTGW-firmware/SATcontrol.ino src/OTGW-firmware/SATcycles.ino
# Expected: empty.
```

**Commit 1 verification**:
- `python build.py --firmware` exits 0.
- `python evaluate.py --quick` clean.
- With `bSimulation = false` and a real boiler attached (manual gate — runs on maintainer's rig), the SAT command stream should be byte-identical to the pre-commit baseline. The wrappers are pure pass-throughs in this state.

**Commit message**:
```
feat(sat): introduce simulation wrappers + migrate read sites

Adds satGetFlowTemp / satGetReturnTemp / satIsFlameOn /
satGetActualModulation. Migrates 19 SAT-internal reads from
OTcurrentSystemState to the wrappers. No behavioural change while
bSimulation=false: each wrapper is a pure pass-through. Sets up the
simulation contract (ADR-NNN, plan §5.1, §7).
```

### 14.2 Commit 2: Boiler model (synthetic flame, modulation, Tret)

**Scope**: §5.2-§5.6. Extends `satUpdateSimulation()` with the flame state machine, modulation, return temp, and the modulation-coupled flow model. Adds the 7 fields to `SATRuntimeSection` (§6). Still no transmit gating or availability gate — those are commit 3.

**State fields — append to `SATtypes.h` inside `SATRuntimeSection`** (after existing `bSimWarmupDone`):

```cpp
// Synthetic boiler-side model (ADR-NNN, plan §6)
bool     bSimFlameOn          = false;
uint8_t  iSimModulation       = 0;       // 0-100 %
float    fSimReturnTemp       = 20.0f;   // °C
uint32_t iSimFlameOnSinceMs   = 0;
uint32_t iSimFlameOffSinceMs  = 0;
// Command trace (used by commit 3 — declared here to keep state additions contiguous)
char     sLastBlockedCmd[24]  = {0};
uint32_t iLastBlockedCmdMs    = 0;
// Edge-hook deferred flag (used by commit 3)
bool     bBoilerDetectedFlag  = false;
```

**Tuning constants — append to `SATcontrol.ino` after the existing `SAT_SIM_*` block (line ~66)**:

```cpp
static const float    SAT_SIM_FLAME_HYST_LO     = 0.5f;
static const uint32_t SAT_SIM_MIN_OFF_MS        = 60000UL;
static const float    SAT_SIM_MOD_KP            = 20.0f;
static const float    SAT_SIM_FLOW_LOSS_K       = 0.02f;   // simple linear loss coefficient
static const float    SAT_SIM_FLOW_THERMAL_GAIN = 0.4f;    // tune so a 24 kW boiler gives ~2 °C/s ramp at full mod
static const uint32_t SAT_BOILER_PRESENCE_WINDOW_MS = 60000UL;
```

**Helpers — add to `SATcontrol.ino`** (near the simulation block):

```cpp
static uint8_t satSimMinMod() {
  return (satGetManufacturerQuirks() & SAT_QUIRK_MIN_MOD_10) ? 10 : 5;
}
```

**Extend `satUpdateSimulation()`** — replace the existing flow + room model with this (preserve the warmup logic comments, integrate flame SM ahead of flow):

```cpp
static void satUpdateSimulation() {
  if (!settings.sat.bSimulation) return;

  uint32_t now = millis();
  if (state.sat.iSimLastUpdateMs == 0) {
    state.sat.iSimLastUpdateMs = now;
    state.sat.bSimWarmupDone = false;
    SATDebugTln(F("SAT SIM: simulation started"));
    return;
  }
  float dtSec = (float)(now - state.sat.iSimLastUpdateMs) / 1000.0f;
  if (dtSec <= 0.0f || dtSec > 60.0f) dtSec = 1.0f;
  state.sat.iSimLastUpdateMs = now;

  // --- Flame state machine (§5.2) ---
  const float sp = state.sat.fFinalSetpoint;
  const bool  heatRequested = state.sat.bActive && sp > (SAT_MIN_SETPOINT + 1.0f);
  const bool  flowBelowSP   = state.sat.fSimFlowTemp <  (sp - SAT_SIM_FLAME_HYST_LO);
  const bool  flowAboveSP   = state.sat.fSimFlowTemp >= (sp + settings.sat.fOvershootMargin);
  const bool  offLongEnough = (now - state.sat.iSimFlameOffSinceMs) >= SAT_SIM_MIN_OFF_MS;
  if (!state.sat.bSimFlameOn) {
    if (heatRequested && flowBelowSP && offLongEnough) {
      state.sat.bSimFlameOn = true;
      state.sat.iSimFlameOnSinceMs = now;
      satCycleOnFlameChange(true);
      SATDebugTln(F("SAT SIM: flame ON"));
    }
  } else {
    if (!heatRequested || flowAboveSP) {
      state.sat.bSimFlameOn = false;
      state.sat.iSimFlameOffSinceMs = now;
      satCycleOnFlameChange(false);
      SATDebugTln(F("SAT SIM: flame OFF"));
    }
  }

  // --- Modulation (§5.3) ---
  if (state.sat.bSimFlameOn) {
    const float err = sp - state.sat.fSimFlowTemp;
    float mod = SAT_SIM_MOD_KP * err;
    if (mod < satSimMinMod()) mod = satSimMinMod();
    if (mod > settings.sat.iMaxRelModulation) mod = settings.sat.iMaxRelModulation;
    state.sat.iSimModulation = (uint8_t)mod;
  } else {
    state.sat.iSimModulation = 0;
  }

  // --- Flow temperature (§5.4, modulation-coupled) ---
  const float power_in  = ((float)state.sat.iSimModulation / 100.0f) * settings.sat.fBoilerCapacity;
  const float heat_loss = SAT_SIM_FLOW_LOSS_K * (state.sat.fSimFlowTemp - state.sat.fSimRoomTemp);
  state.sat.fSimFlowTemp += (power_in - heat_loss) * SAT_SIM_FLOW_THERMAL_GAIN * dtSec;
  if (state.sat.fSimFlowTemp < state.sat.fSimRoomTemp) state.sat.fSimFlowTemp = state.sat.fSimRoomTemp;
  if (state.sat.fSimFlowTemp > settings.sat.fMaxSetpoint) state.sat.fSimFlowTemp = settings.sat.fMaxSetpoint;

  // --- Return temperature (§5.5) ---
  const float delta_tr = 5.0f + 15.0f * ((float)state.sat.iSimModulation / 100.0f);
  state.sat.fSimReturnTemp = state.sat.fSimFlowTemp - delta_tr;
  if (state.sat.fSimReturnTemp < state.sat.fSimRoomTemp) state.sat.fSimReturnTemp = state.sat.fSimRoomTemp;

  // --- Room temperature (§5.6) — heating now keyed on satIsFlameOn() ---
  const float dtMin = dtSec / 60.0f;
  if (state.sat.bSimFlameOn) {
    const float target = settings.sat.fTargetTemp;
    if (state.sat.fSimRoomTemp < target) {
      state.sat.fSimRoomTemp += settings.sat.fSimHeatRate * dtMin;
      if (state.sat.fSimRoomTemp > target) state.sat.fSimRoomTemp = target;
    }
  } else {
    if (state.sat.fSimRoomTemp > state.sat.fSimOutdoorTemp) {
      state.sat.fSimRoomTemp -= settings.sat.fSimCoolRate * dtMin;
      if (state.sat.fSimRoomTemp < state.sat.fSimOutdoorTemp) state.sat.fSimRoomTemp = state.sat.fSimOutdoorTemp;
    }
  }
}
```

**Commit 2 verification**:
- Build + evaluator green.
- Manual: with `bSimulation = true` (no boiler), drop `fTargetTemp` from 20 → 22 → observe `bSimFlameOn` flips true within 1 control loop, `iSimModulation` rises, `iCycleCount` increments.

**Commit message**:
```
feat(sat): synthetic boiler model — flame, modulation, return temp

Extends satUpdateSimulation() with a flame state machine, modulation
controller, modulation-coupled flow temp, and synthetic return temp.
Room temp now keyed on satIsFlameOn() so it only warms while the
synthetic boiler fires. Adds 9 fields to SATRuntimeSection (transient).
Implements plan §5.2-§5.6 + §6.
```

### 14.3 Commit 3: Simulation contract (bus-tx gates + availability + trace + Web UI)

**Scope**: §4 in full. The bus-tx gate helper, the three chokepoint integrations, the edge hook, `satOnBoilerDetected()`, `satBoilerHardwarePresent()`, the runtime backstop, the REST 409, the MQTT reject, the Web UI hide, and the command-trace surfaces.

**14.3.1 Gate helper — add to `SATcontrol.ino`**:

```cpp
// Returns true when the caller must NOT emit on the boiler-side bus.
// Idempotent diagnostic side-effect: writes the would-be command to telnet,
// the §4.3 trace state, and the WebSocket event channel.
static inline bool satSimulationBlocksBusTx(const char* cmd,
                                            const __FlashStringHelper* source) {
  if (!(state.debug.bOTGWSimulation || settings.sat.bSimulation)) return false;
  const char* tag = settings.sat.bSimulation ? "SAT-SIM" : "OTGW-SIM";
  OTDebugTf(PSTR("%s trace: %s (src=%S)\r\n"), tag, cmd, source);
  sendEventToWebSocket_P('!', PSTR("simulation blocked bus tx"));
  if (settings.sat.bSimulation) {
    strlcpy(state.sat.sLastBlockedCmd, cmd, sizeof(state.sat.sLastBlockedCmd));
    state.sat.iLastBlockedCmdMs = millis();
  }
  return true;
}
```

**14.3.2 Availability gate — add to `SATcontrol.ino`**:

```cpp
static bool satBoilerHardwarePresent() {
  if (state.sat.iSlaveMemberID != 0) return true;
  // Replace `otcLastSlaveInboundMs` with the actual symbol you find during the
  // §13.2 reading pass. Likely candidates: OTcurrentSystemState.iLastSlaveMsgMs
  // (PIC path) or otDirect.lastInboundMs (OTDirect path). Use the unified one
  // if it exists; otherwise OR both.
  if (otcLastSlaveInboundMs != 0 &&
      (millis() - otcLastSlaveInboundMs) < SAT_BOILER_PRESENCE_WINDOW_MS) return true;
  return false;
}

static void satOnBoilerDetected() {
  if (!settings.sat.bSimulation) return;
  settings.sat.bSimulation = false;
  writeSettings();
  state.sat.bSimFlameOn         = false;
  state.sat.iSimModulation      = 0;
  state.sat.fSimFlowTemp        = OTcurrentSystemState.Tboiler;
  state.sat.fSimReturnTemp      = OTcurrentSystemState.Tret;
  state.sat.iSimFlameOnSinceMs  = 0;
  state.sat.iSimFlameOffSinceMs = 0;
  state.sat.iSimLastUpdateMs    = 0;
  state.sat.sLastBlockedCmd[0]  = '\0';
  state.sat.iLastBlockedCmdMs   = 0;
  OTDebugTln(F("SAT-SIM: boiler appeared on bus — simulation disabled completely"));
  sendEventToWebSocket_P('!', PSTR("SAT-SIM: boiler appeared, simulation off"));
}
```

**Defer to main loop** — the edge hook may run in or near interrupt context (especially OTDirect frame parsing). Set the flag in the hook; act in the SAT main loop:

```cpp
// In SAT main loop (top of doSAT_period() or equivalent, before satUpdateSimulation):
if (state.sat.bBoilerDetectedFlag) {
  state.sat.bBoilerDetectedFlag = false;
  satOnBoilerDetected();
}
```

**Edge hook** — call from the slave-RX entry of both paths. For the PIC path (`OTGW-Core.ino`), the entry is wherever inbound bytes are parsed into `OTcurrentSystemState` (search for `processOTGWmessage` or the function that writes `Statusflags`/`Tboiler`). For OTDirect (`OTDirect.ino`), the entry is the slave-response callback (search for `onSlaveResponse`, `handleSlaveFrame`, or where `Tboiler` is populated from a parsed frame). Add this one-liner at the top of each:

```cpp
// Edge-trigger SAT availability gate (plan §4.2)
if (settings.sat.bSimulation) state.sat.bBoilerDetectedFlag = true;
```

**14.3.3 Path (a) gate integration — `OTGW-Core.ino` line ~3043**:

The existing `bOTGWSimulation` block becomes:

```cpp
if (satSimulationBlocksBusTx(sWrite, F("pic-serial"))) {
  // helper handled trace + event
  return;
}
// ... existing real-Serial-write path
```

(The variable `sWrite` is the ASCII command buffer the existing gate already references — confirm during reading pass.)

**14.3.4 Path (b) + (c) gate integration — `OTDirect.ino`**:

At the master-frame emit chokepoint (likely `sendMasterRequestAsync` around line 1207), format and gate:

```cpp
char cmdBuf[24];
const uint8_t mid = (frame >> 16) & 0x7F;
const uint16_t val = frame & 0xFFFF;
snprintf_P(cmdBuf, sizeof(cmdBuf), PSTR("MID=%u VAL=%u"), mid, val);
if (satSimulationBlocksBusTx(cmdBuf, F("otdirect-tx"))) {
  return /* whatever success sentinel sendRequestAsync uses */;
}
// ... existing real-bus emit
```

At `probeOTBus()` entry:

```cpp
if (settings.sat.bSimulation || state.debug.bOTGWSimulation) {
  // Synthetic-online so SAT does not flip into eFallbackActive.
  // Availability gate (§4.2) reads a DIFFERENT signal (slave-frame arrival),
  // not boilerOnline — no feedback loop.
  OTcurrentSystemState.boilerOnline = true;
  return true;  // probe succeeded
}
// ... existing real probe
```

For path (c) — the translation/pass-through emitter — first prove during the reading pass whether it converges with (b). If it shares the `sendMasterRequestAsync` chokepoint, no additional gating needed (one gate covers both). If it has a separate emitter (e.g. a `forwardToBoiler()` that bypasses `sendMasterRequestAsync`), add the same pattern with `F("translation")` as the source tag. Document the finding in the commit message.

**14.3.5 REST 409 — `restAPI.ino` SAT settings PATCH handler**:

Find the handler that processes `bSimulation` from inbound JSON. Add at the top of the bSimulation branch:

```cpp
if (parsedValue == true && satBoilerHardwarePresent()) {
  sendApiError(409, F("simulation unavailable: boiler hardware detected"));
  return;
}
```

In the SAT status JSON block (line ~1896), add fields:

```cpp
sendJsonMapEntry(F("sim_available"), !satBoilerHardwarePresent());
if (settings.sat.bSimulation) {
  satSendJsonString(F("last_blocked_cmd"), state.sat.sLastBlockedCmd);
  sendJsonMapEntry(F("last_blocked_cmd_age_ms"),
                   state.sat.iLastBlockedCmdMs == 0 ? 0
                       : (uint32_t)(millis() - state.sat.iLastBlockedCmdMs));
}
```

**14.3.6 MQTT — `MQTTstuff.ino`**:

Find the inbound `sat/simulation/set` handler. Add the same boiler-presence rejection:

```cpp
if (payloadIsOn && satBoilerHardwarePresent()) {
  DebugTln(F("SAT-SIM: MQTT enable rejected — boiler present"));
  // Re-publish current state so HA's optimistic UI snaps back to OFF:
  // (existing shadow s_simulation will pick this up next tick)
  return;
}
```

In the SAT publisher (`s_*` shadow pattern), add publish-on-change for `sat/sim/last_cmd`:

```cpp
static SATShadowS s_last_blocked_cmd;
if (settings.sat.bSimulation) {
  publishIfChangedS(F("sat/sim/last_cmd"), state.sat.sLastBlockedCmd,
                    s_last_blocked_cmd, /*retained=*/false);
}
```

**14.3.7 MQTT discovery — `MQTTHaDiscovery.cpp`**:

Add one entry in the SAT discovery block following the existing pattern; sensor name `sat_last_blocked_cmd`, no unit, no device_class.

**14.3.8 Web UI — `data/index.html`**:

Find the simulation card (search `simulation` in the file). Wrap it:

```html
<div data-sim-only hidden title="Simulation is only available on a bench setup with no boiler attached">
  <!-- existing simulation card markup -->
  <div class="sat-last-blocked-cmd" hidden>
    Last command: <span id="sat-last-cmd"></span>
    <span class="age" id="sat-last-cmd-age"></span>
  </div>
</div>
```

**14.3.9 Web UI — `data/index.js`**:

In the SAT status refresh handler (search for the `fetch('/api/v2/sat/status')` or equivalent):

```javascript
const sim = data.simulation || {};
document.querySelectorAll('[data-sim-only]').forEach(el => {
  el.hidden = !sim.sim_available;
});
const lastCmdEl = document.getElementById('sat-last-cmd');
const lastAgeEl = document.getElementById('sat-last-cmd-age');
if (lastCmdEl && sim.last_blocked_cmd) {
  lastCmdEl.textContent = sim.last_blocked_cmd;
  if (lastAgeEl) lastAgeEl.textContent = `(${Math.round((sim.last_blocked_cmd_age_ms || 0) / 1000)}s ago)`;
  lastCmdEl.closest('.sat-last-blocked-cmd')?.removeAttribute('hidden');
}
```

(Guard each DOM lookup per CLAUDE.md browser-compat rules — `?.` is OK in modern Chrome/FF/Safari, and the optional chain on `closest` matches the project's recent style.)

**Commit 3 verification**: every §10 step, including the field-validation gates that need the maintainer's rig.

**Commit message**:
```
feat(sat): simulation contract — bus isolation, availability, trace

Implements plan §4 in full:
- §4.1 bus-tx gates at all three boiler-side chokepoints (PIC serial,
  OTDirect master TX, OTGW32 translation/pass-through);
- §4.2 edge-triggered availability gate with complete teardown
  (satOnBoilerDetected) + REST 409 + MQTT reject + Web UI hide;
- §4.3 command trace surfaced on telnet, MQTT (sat/sim/last_cmd
  non-retained), REST (last_blocked_cmd + age).

Closes ADR-NNN. Field-validation ACs (8-12, 14) require beta rig.
```

---

## 15. ADR-NNN draft

Run `/adr-kit:adr` (or invoke the `adr-generator` subagent) at the start of commit 3 and have the resulting ADR Accepted before the commit lands. Below is the substantive body to seed the agent — it is **not** a finished ADR (the agent applies the four verification gates).

**File**: `docs/adr/ADR-NNN-sat-simulation-contract.md` (NNN = next free number — `ls docs/adr/ | sort | tail -3`).

```markdown
# ADR-NNN-SAT-simulation-contract

## Status: Proposed

## Context

SAT (Smart Autotune Thermostat) ships with an experimental simulation mode (Task #37) that models room and outdoor temperature, but no boiler-side state. Large parts of the SAT algorithm — cycle classifier, flame health, 4h window stats, daily heating-curve recommendation, OPV calibration — never execute without a real boiler. There is no mechanism stopping a user from enabling simulation while a real boiler is attached, which would let SAT regulate against synthetic state while still sending commands the boiler obeys.

Two distinct hardware platforms drive the boiler-side bus in this branch:
- Any `HAS_PIC` build (ESP8266 + OTGW PIC, ESP32-S3 + PIC variants) talks to the PIC over UART; the PIC owns the OT bus.
- OTGW32 (ESP32-S3 native) drives the OT bus directly via OTDirect, with a translation/pass-through layer for thermostat-originated frames.

Both paths must be safe under simulation.

## Decision

Simulation mode operates under a tri-invariant runtime contract:

1. **Bus isolation.** No bytes physically affecting the heating system leave the device on any of the three actuation paths (PIC serial, OTDirect master TX, OTGW32 translation/pass-through) while `settings.sat.bSimulation` is true. Thermostat-side replies on OTGW32 are unaffected — the cut is strictly boiler-side.
2. **Boiler-absence availability gate.** Simulation is only available when no boiler slave has been observed. The moment a boiler-originated slave frame arrives, simulation is forcibly disabled (settings flipped, persisted, synthetic state torn down, MQTT state flipped to OFF) within ≤ 1 s. REST `PATCH` of `bSimulation:true` returns HTTP 409 while a boiler is present; MQTT `set ON` is rejected; the Web UI hides the simulation card.
3. **Visible command trace.** Every command the SAT loop would have emitted is captured as ASCII at the gate site and surfaced on telnet (`SAT-SIM trace:`), MQTT (`sat/sim/last_cmd`, non-retained), and REST status (`last_blocked_cmd` + `last_blocked_cmd_age_ms`).

Boiler-presence and bus-liveness are read from **independent signals** — slave-frame arrival vs `boilerOnline` — so the synthetic-online used by `probeOTBus()` during simulation cannot feed back into the availability gate.

## Alternatives considered

- **Status quo (do nothing).** Rejected: leaves ~40-60% of SAT control paths untested by simulation; gives no protection against enabling simulation with a real boiler attached.
- **OTGW serial-replay (`bOTGWSimulation`) only.** Rejected: replays historical traffic; cannot exercise control-loop responses to user-driven changes in setpoint, weather, or PV surplus.
- **Wrapper getters without a contract.** Rejected: silently injecting synthetic flow/flame would mask undefined behaviour when a user enables simulation on a production rig — the contract is what makes the feature safe.
- **Per-path gate (gate each emitter individually, no shared helper).** Rejected: duplicating the predicate + trace logic at every chokepoint guarantees drift; a future contributor adding a fourth emitter would not know to copy the gate.

## Consequences

Positive:
- Bench-test of full SAT algorithm becomes possible on hardware-only rigs without a boiler.
- Existing field-deployed devices are unaffected — `bSimulation` defaults false; no UI surface exists on devices with a detected boiler.
- The `'!'`-level WebSocket event and command trace give operators clear visibility into what the regulator would have done.

Negative / risks:
- New runtime invariant for future contributors: every new boiler-side emit path must be gated through `satSimulationBlocksBusTx()`. The Enforcement block below catches the common slip.
- `writeSettings()` on boiler-detect causes one LittleFS flush at the moment of detection — small (one-shot per session).
- Two independent slave-arrival signals (PIC-path + OTDirect-path) must both feed the edge hook. Missing one is a silent failure mode caught by AC #8 and the §10 step 9 verification.

## Related

- Task #37 (original simulation feature)
- Backlog task: feat-2.0.0 SAT simulation contract
- ADR-051 (state/settings split — honoured by the new state.sat fields)
- ADR-076 (OPV calibration — exercised by AC #G6 under sim)

## Enforcement

```json
{
  "forbid_pattern": [
    {
      "pattern": "OTcurrentSystemState\\.(Tboiler|Tret)\\s*[^=]",
      "path_glob": "src/OTGW-firmware/SAT*.ino",
      "message": "Use satGetFlowTemp()/satGetReturnTemp() — ADR-NNN simulation contract."
    },
    {
      "pattern": "Statusflags\\s*&\\s*0x08",
      "path_glob": "src/OTGW-firmware/SAT*.ino",
      "message": "Use satIsFlameOn() — ADR-NNN simulation contract."
    }
  ],
  "require_pattern": [],
  "llm_judge": true
}
```
```

After authoring: `bin/adr-lint docs/adr/ADR-NNN-sat-simulation-contract.md` (must pass Completeness + Consistency). Then human-review → flip Status to `Accepted, YYYY-MM-DD`. Reference ADR-NNN in commit 3 and the backlog task.

---

## 16. Backlog + commit + push + PR templates

### 16.1 Start the task (FIRST thing — before reading any source)

```bash
# Create or pick up the task. If it doesn't exist yet:
backlog task create "feat-2.0.0: SAT simulation contract — boiler model, bus isolation, availability gate, command trace" \
  -d "Implements plan docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md §4-§7. See plan for full design." \
  -l sat,simulation,safety

# Returns an ID like TASK-XXX. Immediately:
backlog task edit XXX -s "In Progress" -a @claude

# Add the AC list from §11 (one --ac per criterion).
backlog task edit XXX \
  --ac "satIsFlameOn / satGetFlowTemp / satGetReturnTemp / satGetActualModulation wrappers exist and route via settings.sat.bSimulation" \
  --ac "All 19 call sites in plan §7 migrated to wrappers" \
  --ac "Synthetic flame edges drive satCycleOnFlameChange and increment iCycleCount; eLastCycleClass reaches non-NONE" \
  --ac "iSimModulation varies between minMod and iMaxRelModulation under varying PID error" \
  --ac "fSimReturnTemp = fSimFlowTemp − delta(mod), floored at fSimRoomTemp" \
  --ac "All three boiler-side actuation paths gated when bSimulation OR bOTGWSimulation; thermostat-side replies permitted; verified on both HAS_PIC and OTGW32 builds" \
  --ac "Command trace appears on telnet (SAT-SIM trace:), MQTT (sat/sim/last_cmd non-retained), REST (last_blocked_cmd + age); covers paths (a)(b)(c)" \
  --ac "Edge-triggered auto-disable: satOnBoilerDetected fires within ≤1s of first slave frame; complete teardown (settings flipped+persisted, synthetic reset, trace cleared, MQTT OFF, telnet event); survives reboot; verified on both HAS_PIC and OTGW32" \
  --ac "REST PATCH bSimulation:true while boiler present returns HTTP 409 with documented body" \
  --ac "MQTT sat/simulation/set ON while boiler present rejected with telnet warning; state topic does not flip" \
  --ac "Web UI: simulation card + diagnostic block hidden when sim_available=false; visible when true" \
  --ac "Standalone bench (no thermostat, no boiler): full SAT loop runs; AC 3-5, 7 all observable" \
  --ac "python build.py --firmware exits 0 for both HAS_PIC and OTGW32 targets; python evaluate.py --quick clean" \
  --ac "Real-boiler regression (bSimulation=false): SAT OT command stream byte-identical to pre-change baseline over 10 min" \
  --ac "ADR-NNN (SAT simulation contract) authored and Accepted before merge; Enforcement block included"

# Add the implementation plan to the task:
backlog task edit XXX --plan "$(printf '1. Pre-flight + reading pass (plan §13)\n2. Commit 1: wrappers + call-site migration (plan §14.1)\n3. Commit 2: boiler model (plan §14.2)\n4. Author + Accept ADR-NNN (plan §15)\n5. Commit 3: simulation contract (plan §14.3)\n6. Build + evaluator green; backlog task ACs checked\n7. Push + draft PR')"
```

### 16.2 Per-commit dance

After each commit (1, 2, 3):
1. `python build.py --firmware` — must exit 0.
2. `python evaluate.py --quick` — must show no new findings.
3. `git diff --stat HEAD~1` — sanity-check size matches plan §8 LOC budget.
4. `backlog task edit XXX --append-notes "$(date -Iseconds): commit N landed — <one-liner>"`.

### 16.3 Push

After commit 3 + ACs 1-7, 13 checked, push (standing permission per CLAUDE.md):

```bash
git push -u origin feature-dev-2.0.0-otgw32-esp32-sat-support
```

Retry policy if push fails (per CLAUDE.md): up to 4× with 2/4/8/16 s back-off, only on network errors. Not on rejected-non-fast-forward — that means someone else pushed and you need to rebase.

### 16.4 PR (draft)

```bash
gh pr create --draft --title "feat-2.0.0: SAT simulation contract" --body "$(cat <<'EOF'
## Summary
- Synthetic boiler model (flame SM, modulation, flow/return temp) so SAT cycle classifier, OPV calibration, and heating-curve recommendation execute under simulation.
- Simulation contract: bus isolation on all 3 boiler-side paths (PIC serial, OTDirect TX, OTGW32 translation), edge-triggered availability gate with full teardown when a boiler appears, visible command trace on telnet / MQTT / REST.
- Web UI hides the simulation card when a boiler is detected; REST + MQTT reject enable attempts.

## Test plan
- [x] `python build.py --firmware` (both HAS_PIC and OTGW32 targets)
- [x] `python evaluate.py --quick`
- [ ] Bench rig (no boiler): flame events, modulation, return-flow delta, OPV calibration, command-trace surfaces (§10 steps 3-8)
- [ ] Boiler-attached rig: edge-triggered auto-disable + REST 409 + MQTT reject + Web UI hide (§10 step 9)
- [ ] Standalone bench (no thermostat, no boiler): full SAT loop (§10 step 10)
- [ ] Real-boiler regression: SAT command stream byte-identical with `bSimulation=false` (§10 step 11)

Plan: `docs/plan/SAT_SIMULATION_CONTRACT_PLAN.md`
ADR: `docs/adr/ADR-NNN-sat-simulation-contract.md` (Accepted)
Backlog: TASK-XXX
EOF
)"
```

After PR creation, surface the URL to the user.

---

## 17. Pitfalls and gotchas (read before commit 3)

1. **`writeSettings()` is heavy.** Calling it on every slave frame would thrash LittleFS. The deferred-flag pattern in §14.3 (set flag in hook, act in main loop, run `satOnBoilerDetected()` once) ensures it fires exactly once per detection event. **Verify** during code review that the flag is reset before `satOnBoilerDetected()` returns — otherwise it loops.

2. **`bOTGWSimulation` is a separate, pre-existing feature.** Do not break it. Its semantics (PIC serial replay from `/otgw_simulation.log`) are orthogonal to `bSimulation`. The gate helper handles both via OR, but they must remain independently toggleable from telnet / REST.

3. **The synthetic-online for `probeOTBus()` is delicate.** It must (a) prevent SAT falling into `eFallbackActive` and (b) NOT feed back into `satBoilerHardwarePresent()`. The split-signal design (slave-arrival vs `boilerOnline`) is what enforces (b). If you ever consolidate the two signals, the gate becomes self-disabling — explicit test in §10 step 9 catches this regression.

4. **Path (c) localisation is the riskiest mapping.** Don't trust line numbers from the plan — they may have drifted. During the reading pass (§13.2 step 6), trace every code path from "OT frame received on thermostat side" → "OT frame emitted on boiler side". If (c) shares the same emit chokepoint as (b), say so in the commit message and gate once. If it has a separate emitter, gate both. If you find a third path no one anticipated, **stop and ask the user** before proceeding.

5. **PROGMEM discipline.** All new strings: `F("…")` or `PSTR("…")`. Format strings: `snprintf_P`. No bare `String` class in hot paths (CLAUDE.md ADR-004). The `cmd` buffer in `satSimulationBlocksBusTx` is RAM (intentional — it's caller-supplied), but the `source` is `__FlashStringHelper*` (PROGMEM).

6. **Browser compatibility.** New JS must guard DOM lookups (`?.`, `if (el)`), wrap `JSON.parse` in try/catch, check `response.ok` on fetch. CLAUDE.md frontend rules.

7. **MQTT shadow state for `last_blocked_cmd`.** Use the existing `SATShadowS` pattern (publishes on change), not a forced publish every loop — otherwise broker spam during sim runs.

8. **REST 409 body format.** Match the existing `sendApiError` convention exactly. The body is JSON; the message is in `F("...")`. Find one existing `sendApiError(409, ...)` call site and copy the shape.

9. **Backlog CLI cross-tree behaviour.** Per CLAUDE.md, `backlog task edit` only writes to the worktree where the task file lives. If creating the task fails or edits return "Task not found", check `find . -name "task-XXX*"` and run from the worktree that holds it.

10. **Don't merge the 3 commits.** Keep them separate in the PR — reviewer reads them in order. The split is the point.

---
