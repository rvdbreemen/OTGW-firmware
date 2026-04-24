# ADR-069: SAT PID v3 Controller Implementation

**Status:** Accepted
**Date:** 2026-04-09

## Context

The SAT (Smart Autotune Thermostat) heating controller requires a room-temperature feedback loop on top of the weather-compensated heating curve. A pure heating curve is open-loop: it sets a boiler flow temperature based on outdoor temperature and a target room temperature, but does not correct for deviations caused by solar gain, internal heat sources, thermal mass, or inaccurate curve calibration.

A closed-loop PID controller adds the missing correction term. The choice of PID variant is critical because:

- **ESP8266 has ~40KB usable RAM**: all state must be static and minimal
- **Control cycles run every 30–600 seconds** (configurable), not the millisecond loops typical of industrial PID
- **The heating curve already handles large errors**: outside the deadband, the curve supplies the correct setpoint. The PID only fine-tunes inside or near the deadband
- **Integral windup is a practical concern**: slow heating systems can accumulate large integrals during warm-up that cause sustained overshoot
- **Derivative noise**: room temperature sensors update slowly and may have quantization noise; a naive discrete derivative is unusable

The SAT Python project (https://github.com/Alexwijn/SAT, thermo-nova branch) developed "PID v3" over multiple iterations specifically for slow residential heating systems. This firmware is a C port of that algorithm.

### Alternatives considered

1. **Standard PID with fixed gains** — Rejected: gains depend heavily on the heating curve coefficient (which varies per installation) and heating system type. A single set of fixed gains either over- or under-corrects for most users.
2. **PI controller only (no derivative)** — Rejected: the derivative term in SAT v3 is temperature-based (not error-based) and acts as a damper when room temperature rises rapidly (e.g., solar gain). Omitting it would cause overshoot in those conditions.
3. **PID with separate auto-tuning phase** — Rejected: auto-tuning requires deliberate step disturbances to the system, which is disruptive and potentially uncomfortable. Gain derivation from the heating curve coefficient avoids this entirely.
4. **External PID in Home Assistant** — Rejected: this is the scenario SAT was designed to avoid. An embedded PID operates independently of HA, surviving HA reboots and network outages.
5. **Model Predictive Control (MPC)** — Rejected: requires a thermal model of the building, which is not available at commissioning and takes days to learn. MPC is significantly more complex and RAM-intensive.

## Decision

Implement SAT PID v3 as defined in the SAT Python `pid.py` (thermo-nova branch), ported to C in `SATpid.ino`.

### Controller structure

The output is not a raw correction but a complete boiler setpoint:

```
output = heatingCurveValue + P + I + D
```

This "heating curve plus corrections" model means the heating curve handles coarse control and the PID trims the result. The controller never needs to work from zero.

### Key design decisions

**Auto-gain derivation from heating curve coefficient:**

Gains are computed from the current heating curve value and coefficient at each control cycle, not stored as tunable parameters:

```
Kp = (coeff * curveValue) / divisor    (divisor = 4 for underfloor, 3 for radiators)
Ki = Kp / 8400
Kd = 0.07 * 8400 * Kp
```

This means gains automatically scale with outdoor temperature (which changes `curveValue`) and the user-configured curve coefficient. A user who adjusts the curve coefficient immediately gets retuned PID gains without any additional configuration.

**Fixed 60-second integration window:**

The integral accumulates `Ki * error * 60` per update cycle, regardless of the actual elapsed time. This matches the SAT Python convention and provides consistent integral behavior independent of the configured control interval.

**Deadband-based integral and derivative behavior:**

Inside the deadband (`|error| <= deadband`, default 0.25°C):
- Integral accumulates as a compensator for background heat sources (solar gain, occupant activity)
- Integral is clamped to `[0, curveValue]` and hard-capped at 20°C to prevent runaway
- Derivative FREEZES at its last computed value (acts as a static offset, not a dynamic term)

Outside the deadband:
- Integral resets to zero (the heating curve takes over)
- Derivative actively updates

This behavior is deliberately asymmetric from a classical PID: inside the deadband the system is near setpoint and needs only small adjustments; outside it, the curve drives the setpoint and integral windup must be prevented.

**Temperature-based derivative (not error-based):**

The derivative is computed from room temperature change, not error change:

```
rawDeriv = -(roomTemp - prevRoomTemp) / deltaTime
```

The negative sign means a rising room temperature produces a negative derivative, which reduces the PID output and acts as a damper. This avoids derivative kick on setpoint changes (a common problem with error-based derivatives) and is less sensitive to setpoint noise.

**Adaptive alpha low-pass filter:**

```
alpha = deltaTime / (PID_UPDATE_INTERVAL + deltaTime)
```

When `deltaTime` equals the update interval (60s), `alpha = 0.5`. Longer gaps (e.g., after a control pause) give higher alpha, trusting the new reading more. This prevents the filter from being over-smoothed when samples are infrequent.

**Hard derivative cap at ±5°C/s:**

Before filtering, extreme derivative values are clamped to prevent a single noisy sensor reading from dominating the output.

**Solar gain freeze:**

When `state.sat.bSolarGainActive` is set (determined externally based on sunshine duration or irradiance), integral accumulation is suspended. This prevents the integral from growing during solar gain periods, which would cause overshoot when the sun disappears.

**Sample time guard:**

The controller enforces a minimum 10-second gap between updates and skips updates where the error has not changed by more than 0.001°C. This prevents accumulating integral/derivative updates from rapid re-entry (e.g., multiple OT frames arriving in quick succession).

### State variables

All PID state is in static file-scope variables in `SATpid.ino` (not in the global `state` struct), preventing accidental external modification:

- `_pid_lastError`, `_pid_previousError`: error tracking
- `_pid_integral`: accumulated integral (only field with real windup risk)
- `_pid_rawDerivative`: filtered derivative
- `_pid_lastRoomTemp`, `_pid_lastCurveValue`: derivative and gain base values
- `_pid_lastUpdateMs`, `_pid_lastDerivativeMs`: timing

Public state copies in `state.sat.fPidP/I/D/Output/Kp/Ki/Kd/fRawDerivative/fError` are updated each cycle for telemetry and MQTT publishing.

### Reset behavior

`satPidReset()` zeros all state and clears the initialized flag. Called when SAT is disabled, safety trips, or the heating curve coefficient changes significantly.

## Consequences

### Benefits

- No manual PID tuning required at commissioning; gains derive automatically from the heating curve coefficient
- Integral windup is structurally prevented: the deadband boundary resets the integral before it can accumulate dangerously
- Derivative does not cause setpoint-change spikes (temperature-based, not error-based)
- Solar gain is handled without requiring an irradiance sensor: the freeze flag is set by configurable logic
- All state is static; no heap allocation, no RAM fragmentation risk

### Trade-offs

- The 60-second fixed integration window is a departure from standard PID theory; it ties integral accumulation to a wall-clock constant rather than elapsed time. This is intentional for consistency with SAT Python but means the integral "weight" depends implicitly on the update frequency
- The asymmetric deadband behavior (integral resets outside deadband) is non-standard. Systems with large persistent errors (e.g., a badly calibrated heating curve) will never accumulate a corrective integral, relying instead on the curve alone
- Auto-gain derivation means gains change continuously with outdoor temperature. This is by design but makes it harder to reason about transient stability at extreme temperatures

### Risks

- The `SAT_PID_AGGRESSION_V3 = 8400` constant was empirically chosen in the SAT Python project and may not be optimal for all boiler types. The manufacturer quirk system (ADR-085) can partially compensate, but the constant itself is not configurable
- Solar gain detection relies on external state (`bSolarGainActive`) that must be set by the control loop. If this flag is stale or incorrect, the integral freeze mechanism misfires

## Related

- ADR-085: SAT Smart Autotune Thermostat Integration (overall SAT architecture)
- ADR-069 (this ADR): PID implementation
- `SATpid.ino`: Implementation
- `OTGW-firmware.h`: `state.sat` fields for PID telemetry
- SAT upstream: https://github.com/Alexwijn/SAT (thermo-nova branch, `pid.py`)
