# ADR-085: SAT (Smart Autotune Thermostat) Integration

**Status:** Accepted
Renumbered from ADR-062 on 2026-04-24 to resolve duplicate numbering (TASK-412). Content unchanged.
**Date:** 2026-04-02

## Context

The OTGW firmware relays OpenTherm traffic between a thermostat and boiler but has no built-in heating intelligence. Users who want weather-compensated flow-temperature control, PID-based room regulation, or PWM flame cycling must rely entirely on Home Assistant or another external automation platform. If HA is offline, reboots, or loses WiFi connectivity, the boiler falls back to whatever the physical thermostat dictates — which may be a simple on/off device with no modulation.

The SAT (Smart Autotune Thermostat) project (https://github.com/Alexwijn/SAT) implements these features as a Home Assistant integration. Porting the core algorithms into the ESP firmware allows the gateway to function as a standalone smart thermostat, independent of any external controller.

### Key constraints:
- ESP8266 has ~40KB usable RAM; all algorithms must use static buffers and fixed-point where practical
- The PIC serial link is the only path to the boiler; all commands go through `addOTWGcmdtoqueue()`
- Safety is paramount: a firmware bug must not leave the boiler running uncontrolled
- The existing OTGW relay functionality must not be affected when SAT is disabled
- External sensors (MQTT/REST) are optional; the system must work with OT bus data alone
- Settings must persist across reboots; runtime state is RAM-only

### Alternatives considered:
1. **Keep SAT in HA only** — Rejected: single point of failure, high latency, no standalone operation
2. **Port full SAT Python codebase** — Rejected: too complex, many HA dependencies, not suitable for embedded
3. **Selective port of core algorithms** — Chosen: heating curve, PID v3, cycle tracker, PWM/continuous modes

## Decision

Integrate SAT as an embedded heating controller within the OTGW firmware, split across four source files following the existing modular `.ino` architecture (ADR-002):

### Source structure

| File | Responsibility |
|------|---------------|
| `SATcontrol.ino` | Main control loop, safety system, boiler status, mode application, sensor handlers, REST/MQTT publishing |
| `SATpid.ino` | PID v3 controller with auto-gains, deadband mode switching, integral windup protection |
| `SATcycles.ino` | Flame cycle tracker, classifier (good/overshoot/underheat/short/uncertain), PWM auto-switch logic |
| `OTGW-firmware.h` | All SAT types, enums, settings struct, runtime state struct, forward declarations |

### Control architecture

1. **Heating curve**: `setpoint = base_offset + (coeff/4) * [4*(Ttarget-20) + 0.03*(Toutside-20)² - 0.4*(Toutside-20)]` — weather-compensated flow temperature calculation
2. **PID v3**: Proportional + Integral + Derivative correction on top of the heating curve. Auto-gains derived from heating curve coefficient. Deadband suppresses integral windup near setpoint. Hard integral cap at ±20°C.
3. **Dual control modes**:
   - *Continuous*: Sends `CS=<setpoint>` to override boiler flow temperature; boiler modulates internally
   - *PWM*: Cycles flame on/off at calculated duty cycle for boilers that don't modulate well at low demand
4. **Cycle tracker**: Monitors flame ON/OFF transitions, classifies each cycle, and drives automatic switching between PWM and continuous modes based on sustained overshoot, underheat, or saturation

### Safety system (defense in depth)

All safety mechanisms are RAM-only — no flash writes required:

| Protection | Mechanism |
|-----------|-----------|
| Boot safety | `CS=0` sent in `initSAT()` + deferred one-shot for late PIC availability |
| Disable safety | `satDisable()` sends `CS=0` to release boiler control |
| Stale sensor fallback | External temps auto-expire (indoor: 5min, outdoor: 10min), fall back to OT bus |
| Hard temperature ceiling | 50°C underfloor / 80°C radiator, enforced every control cycle |
| Consecutive skip counter | After 10 consecutive skipped cycles, trips safety (sends `CS=0`) |
| PIC comm failure | After 5 consecutive command failures, trips safety |
| Safety trip recovery | Manual re-enable required (toggle SAT off/on via settings) |

### Settings and state (follows ADR-051)

- `settings.sat.*` — 11 persistent fields (bEnabled, iHeatingSystem, fTargetTemp, fHeatingCurveCoeff, fDeadband, iControlInterval, bUseExternalTemp, fPresetComfort/Eco/Away, bPwmAutoSwitch)
- `state.sat.*` — ~33 transient runtime fields (PID state, cycle data, safety flags, boiler status)

### API surface

- **REST**: `GET /api/v2/sat/status`, `POST /api/v2/sat/target`, `POST /api/v2/sat/externaltemp`, `POST /api/v2/sat/externaloutdoor`
- **MQTT**:
  - Subscribe: `<mqtt-prefix>/set/<node-id>/sat/{target,indoor_temp,outdoor_temp,enabled,control_mode}`
  - Publish: `<mqtt-prefix>/value/<node-id>/sat/{mode,setpoint,heating_curve,pid_output,target,error,pid_p,pid_i,pid_d,boiler_status,cycle_class,pwm_duty,safety_tripped}`
- **HA auto-discovery**: Climate entity + 7 sensor entities via `mqttha.cfg`
- **Web UI**: Dedicated SAT dashboard tab (`sat.js`) — visualization only, polls status every 5s

## Consequences

### Benefits
- Gateway operates as a standalone smart thermostat without HA dependency
- Fast recovery after reboot (~30s from persisted settings)
- External sensors are optional enhancements, not requirements
- Safety system ensures `CS=0` (boiler off) is the default failure state
- Web UI provides real-time visibility into control decisions

### Trade-offs
- ~3KB additional flash for SAT code across four files
- ~200 bytes additional RAM for runtime state and cycle history buffer
- Settings JSON grows by ~300 bytes
- Control loop adds ~2ms execution time every 30s (configurable interval)

### Risks
- PID tuning may need adjustment for different boiler types — mitigated by auto-gains derived from heating curve coefficient
- PWM mode may not suit all boilers — mitigated by auto-switch logic and manual mode override
- Arduino `.ino` concatenation ordering means `SATcontrol.ino` compiles before `SATcycles.ino` — mitigated by accessor functions for cross-file data access

## Related

- ADR-002: Modular .ino Architecture (file split pattern)
- ADR-006: MQTT Integration Pattern (topic structure)
- ADR-016: OpenTherm Command Queue (`addOTWGcmdtoqueue`)
- ADR-019: REST API Versioning Strategy (v2 endpoints)
- ADR-041: JIT HA Discovery (auto-discovery config)
- ADR-051: Dual Encapsulating Structs (settings/state pattern)
- SAT upstream: https://github.com/Alexwijn/SAT
