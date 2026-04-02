# SAT: Smart Autotune Thermostat

## What is SAT?

SAT turns your OpenTherm Gateway into a standalone smart thermostat. Instead of relying on your wall thermostat's simple on/off switching, SAT calculates the exact water temperature your boiler should produce based on the current weather, your desired room temperature, and how your home is actually responding. The result: more stable room temperatures, less gas consumption, and fewer boiler start/stop cycles.

### Before SAT

Your wall thermostat tells the boiler "I want heat" or "stop heating." The boiler fires up to its maximum temperature, heats the room past the setpoint, shuts off, the room cools down, and the cycle repeats. This on/off pattern wastes energy, creates temperature swings, and wears out the boiler through frequent ignition cycles.

### With SAT

SAT continuously adjusts the boiler's flow temperature so it produces just enough heat to maintain your target room temperature. On a mild autumn day, the boiler might run at 35°C continuously. On a freezing winter night, it might run at 55°C. The room temperature stays steady within fractions of a degree, and the boiler runs longer at lower intensity — which is exactly how condensing boilers are designed to operate most efficiently.

### Key properties

- **Standalone**: Runs entirely on the ESP microcontroller. No Home Assistant, no cloud, no internet required.
- **Automatic**: Calculates its own PID gains from the heating curve. No manual tuning needed.
- **Safe**: Six independent safety layers ensure the boiler is always released (`CS=0`) when something goes wrong.
- **Compatible**: Works with any OpenTherm boiler. Supports both radiator and underfloor heating systems.
- **Observable**: Full status visible via Web UI dashboard, REST API, MQTT, and Home Assistant auto-discovery.

---

## System Overview

SAT sits between the OpenTherm data stream and the PIC gateway chip. It reads temperatures from the OpenTherm bus (or optional external sensors), computes an optimal boiler flow temperature, and sends a `CS=` (Control Setpoint) command to override the thermostat's request.

```
                        ┌─────────────────────────────────────────────┐
                        │              ESP8266 Firmware               │
                        │                                             │
  ┌──────────┐          │  ┌─────────────┐    ┌───────────────────┐   │
  │  Wall    │◄────OT───┤  │  OpenTherm  │    │   SAT Controller  │   │
  │Thermostat│    bus    │  │   Monitor   │───►│                   │   │
  └──────────┘          │  │ (msg relay) │    │ ┌───────────────┐ │   │
                        │  └─────────────┘    │ │ Heating Curve │ │   │
  ┌──────────┐          │        │            │ └───────┬───────┘ │   │
  │  Boiler  │◄────OT───┤        │            │         │         │   │
  │          │    bus    │        ▼            │ ┌───────▼───────┐ │   │
  └──────────┘          │  ┌───────────┐      │ │   PID v3      │ │   │
                        │  │OTcurrent- │      │ │  Controller   │ │   │
                        │  │SystemState│─────►│ └───────┬───────┘ │   │
                        │  │           │      │         │         │   │
                        │  │ Tr, Tout,  │      │ ┌───────▼───────┐ │   │
                        │  │ Tboiler,  │      │ │ Mode Apply    │ │   │
                        │  │ TSet,     │      │ │ (cont / PWM)  │ │   │
                        │  │ flame,    │      │ └───────┬───────┘ │   │
                        │  │ modulation│      │         │         │   │
                        │  └───────────┘      │ ┌───────▼───────┐ │   │
                        │                     │ │ Safety Clamp  │ │   │
                        │                     │ └───────┬───────┘ │   │
                        │                     └─────────┼─────────┘   │
                        │                               │             │
                        │                        CS=xx.x│             │
                        │                               ▼             │
                        │                     ┌──────────────┐        │
                        │                     │  PIC Gateway │        │
                        │                     │  (serial)    │        │
                        │                     └──────────────┘        │
                        └─────────────────────────────────────────────┘
                                      │
                      ┌───────────────┼───────────────┐
                      │               │               │
                ┌─────▼─────┐  ┌──────▼──────┐  ┌────▼────┐
                │  REST API │  │    MQTT     │  │ Web UI  │
                │/api/v2/sat│  │ sat/target  │  │sat.js   │
                │           │  │ sat/mode    │  │dashboard│
                └───────────┘  └─────────────┘  └─────────┘
```

### Data flow per control cycle (every 30s default)

1. **Read inputs**: Room temp, outside temp (OT bus or external sensor), target temp (settings)
2. **Validate**: Check sensor staleness, range validity, consecutive failure count
3. **Heating curve**: Calculate base flow setpoint from outside temp and target temp
4. **PID correction**: Apply proportional + integral + derivative correction
5. **Mode apply**: Continuous mode clamps to flow temp; PWM mode converts to duty cycle
6. **Safety clamp**: Enforce hard ceiling (50°C floor / 80°C radiator), min 10°C
7. **Send command**: Queue `CS=xx.x` to PIC gateway
8. **Publish**: Send status to MQTT and Web UI

### Source file organization

| File | Lines | Responsibility |
|------|-------|----------------|
| `SATcontrol.ino` | ~580 | Control loop, safety, boiler status, sensor handlers, MQTT/REST |
| `SATpid.ino` | ~210 | PID v3 with auto-gains, deadband switching |
| `SATcycles.ino` | ~235 | Flame cycle tracker, classifier, auto-switch logic |
| `OTGW-firmware.h` | (additions) | Enums, settings struct, runtime state struct, forward declarations |

---

## The Heating Curve

The heating curve is the foundation of weather-compensated heating. It answers the question: *"Given the current outside temperature, what flow temperature does the boiler need to maintain the target room temperature?"*

### Intuition

On a warm day (15°C outside), the house loses little heat, so the boiler only needs lukewarm water (e.g., 30°C). On a freezing day (-10°C outside), heat loss is enormous, so the boiler needs much hotter water (e.g., 60°C). The heating curve maps this relationship mathematically.

### The formula

```
setpoint = base_offset + (coefficient / 4) * curve_value

where:
  curve_value = 4 * (T_target - 20)
              + 0.03 * (T_outside - 20)^2
              - 0.4 * (T_outside - 20)
```

**Parameters**:

| Parameter | Radiator | Underfloor | Description |
|-----------|----------|------------|-------------|
| `base_offset` | 27.2°C | 20.0°C | Flow temp when curve_value = 0 |
| `coefficient` | 0.5 - 3.0 (typical 1.5) | 0.5 - 2.0 (typical 1.0) | Steepness of the curve |
| `T_target` | User setting | User setting | Desired room temperature |
| `T_outside` | OT bus or sensor | OT bus or sensor | Current outdoor temperature |

The quadratic term `0.03 * (T_outside - 20)^2` models the non-linear increase in heat loss at very cold temperatures — a house loses proportionally more heat at -15°C than at +5°C.

### Worked examples (radiator system, coefficient = 1.5)

| Outside temp | Target | curve_value | Setpoint | Meaning |
|-------------|--------|-------------|----------|---------|
| 15°C | 20°C | 0 + 0.75 + 2.0 = 2.75 | 27.2 + 1.03 = **28.2°C** | Mild day, very low flow temp |
| 5°C | 20°C | 0 + 6.75 + 6.0 = 12.75 | 27.2 + 4.78 = **32.0°C** | Cool day, moderate flow temp |
| -5°C | 20°C | 0 + 18.75 + 10.0 = 28.75 | 27.2 + 10.78 = **38.0°C** | Cold day, higher flow temp |
| -10°C | 21°C | 4 + 27.0 + 12.0 = 43.0 | 27.2 + 16.13 = **43.3°C** | Freezing, significant heating |
| -20°C | 21°C | 4 + 48.0 + 16.0 = 68.0 | 27.2 + 25.50 = **52.7°C** | Extreme cold, near max |

### Curve shape visualization

```
  Flow temp (°C)
  70 ┤
     │                                              coefficient = 2.5
  60 ┤                                           ╱
     │                                        ╱
  50 ┤                                     ╱      coefficient = 1.5
     │                                  ╱      ╱
  40 ┤                              ╱       ╱
     │                           ╱       ╱       coefficient = 0.8
  35 ┤                        ╱       ╱       ╱
     │                     ╱       ╱       ╱
  30 ┤─ ─ ─ ─ ─ ─ ─ ─ ╱─ ─ ─ ╱─ ─ ─ ╱─ ─ ─ ─ base offset (rad)
     │              ╱       ╱       ╱
  25 ┤           ╱       ╱       ╱
     │        ╱       ╱       ╱
  20 ┤─ ─ ╱─ ─ ─ ╱─ ─ ─ ╱─ ─ ─ ─ ─ ─ ─ ─ ─ ─ base offset (floor)
     │
     ├────┬────┬────┬────┬────┬────┬────┬────┬──
    -20  -15  -10   -5    0    5   10   15   20
                    Outside temperature (°C)
```

Higher coefficient = steeper curve = more aggressive heating in cold weather. The coefficient is the single most important tuning parameter: if your house is well-insulated, use a lower coefficient (0.8-1.2). Older, drafty houses need a higher coefficient (1.5-2.5).

### Coefficient selection guide

| Insulation level | Building type | Suggested coefficient |
|------------------|---------------|----------------------|
| Excellent | Modern passive house, HR++ glass | 0.5 - 0.8 |
| Good | Post-2000 build, cavity wall insulation | 0.8 - 1.2 |
| Average | 1970s-2000s, partial insulation | 1.2 - 1.8 |
| Poor | Pre-1970, single glazing, no cavity | 1.8 - 2.5 |

---

## PID v3 Controller

The heating curve provides a good baseline flow temperature, but it cannot account for internal heat gains (cooking, sunlight, people), opening windows, or the thermal lag of the building. The PID controller adds a correction on top of the heating curve to close this gap.

### Output structure

```
PID_output = heating_curve_value + P + I + D
```

The PID output is not a standalone value — it is a **correction applied on top of the heating curve**. This is the key insight of the thermo-nova approach: the heating curve does the heavy lifting, and the PID fine-tunes it.

### Auto-gain calculation

Unlike traditional PID controllers where gains are manually tuned, SAT v3 calculates Kp, Ki, and Kd automatically from the heating curve coefficient and the current curve output value:

```
Kp = (coefficient * curve_value) / divisor
Ki = Kp / 8400
Kd = 0.07 * 8400 * Kp
```

Where `divisor` is 3 for radiators and 4 for underfloor heating (underfloor systems respond more slowly, so gains are more conservative).

**Why this works**: The heating curve coefficient already encodes how responsive your heating system needs to be. A high coefficient (poorly insulated house) means the system needs to react more aggressively — and the auto-gains scale proportionally. The constant 8400 (the "aggression factor") sets the ratio between P, I, and D terms and was tuned empirically across many installations.

### Deadband mode switching

The PID uses the deadband (default 0.25°C) to switch between two operating regimes:

```
  error = target_temp - room_temp

  ┌──────────────────────────────────────────────────────┐
  │                                                      │
  │  |error| > deadband         |error| <= deadband      │
  │  ─────────────────         ──────────────────────     │
  │                                                      │
  │  P term: ACTIVE             P term: ACTIVE           │
  │  I term: RESET to 0         I term: ACCUMULATING     │
  │  D term: ACTIVE             D term: DISABLED         │
  │                                                      │
  │  Goal: Get to setpoint      Goal: Stay at setpoint   │
  │  fast, dampen overshoot     eliminate steady-state    │
  │                             error                    │
  └──────────────────────────────────────────────────────┘
```

**Why split this way?**

- **Far from setpoint** (|error| > deadband): The derivative term dampens the approach rate, preventing overshoot. The integral would accumulate a large value during the approach that would then cause overshoot — so it is held at zero.
- **Near setpoint** (|error| <= deadband): The derivative becomes noise-dominated at small temperature changes, so it is disabled. The integral slowly corrects any remaining steady-state offset that the P term alone cannot eliminate.

### Proportional term (P)

```
P = Kp * error
```

The proportional term provides immediate response proportional to the error. If the room is 0.5°C below target, P increases the flow temperature. If the room is 0.2°C above target, P decreases it.

### Integral term (I)

```
I += Ki * error * delta_time
```

The integral only accumulates when `|error| <= deadband`. It has three protection mechanisms against windup:

1. **Curve-value clamp**: `|I| <= curve_value` — the integral correction cannot exceed the base heating curve output
2. **Hard absolute cap**: `|I| <= 20°C` — defense against runaway regardless of curve value
3. **Reset on transition**: When error crosses from outside to inside the deadband, the integral timer resets

The integral also has a time limit (`SAT_PID_INTEGRAL_TIME_LIMIT = 300s`): it only begins accumulating after 5 minutes of being within the deadband. This prevents the integral from responding to brief fluctuations.

### Derivative term (D)

```
raw_derivative = (room_temp - previous_room_temp) / delta_time
filtered = 0.8 * raw_derivative + 0.2 * previous_filtered
D = Kd * filtered
```

The derivative is **temperature-based**, not error-based. This is a deliberate design choice from the thermo-nova approach:

- Error-based derivative responds to target changes (setpoint jumps), causing unnecessary spikes
- Temperature-based derivative only responds to actual room temperature changes

The derivative uses a single-pole exponential moving average (EMA) filter with alpha=0.8 and a magnitude cap of ±5.0°C/s to reject sensor noise and spikes.

### PID signal flow

```
               ┌─────────┐
  target_temp ─┤         │
               │  error  ├──┬────────────┬──────────────────┐
  room_temp  ──┤ = t - r │  │            │                  │
               └─────────┘  │            │                  │
                             ▼            ▼                  ▼
                        ┌────────┐  ┌──────────┐      ┌──────────┐
                        │ P term │  │  I term  │      │  D term  │
                        │Kp*error│  │accumulate│      │temp-based│
                        │        │  │if in band│      │if out of │
                        │ always │  │          │      │ deadband │
                        │ active │  │ 3 clamps │      │EMA + cap │
                        └───┬────┘  └────┬─────┘      └────┬─────┘
                            │            │                  │
                            ▼            ▼                  ▼
                        ┌────────────────────────────────────────┐
                        │                                        │
  heating_curve_value ──┤   output = curve + P + I + D           │
                        │                                        │
                        └────────────────┬───────────────────────┘
                                         │
                                         ▼
                                   PID output (°C)
```

### Example scenario

Room target: 21.0°C, outside: 5°C, coefficient: 1.5 (radiator)

| Time | Room temp | Error | Curve | P | I | D | Output |
|------|-----------|-------|-------|---|---|---|--------|
| 0min | 19.5°C | +1.5 | 32.0 | +7.8 | 0 | 0 | 39.8°C |
| 10min | 20.2°C | +0.8 | 32.0 | +4.2 | 0 | -1.2 | 35.0°C |
| 20min | 20.7°C | +0.3 | 32.0 | +1.6 | 0 | -0.8 | 32.8°C |
| 30min | 20.9°C | +0.1 | 32.0 | +0.5 | 0.1 | 0 | 32.6°C |
| 60min | 20.95°C | +0.05 | 32.0 | +0.3 | 0.3 | 0 | 32.6°C |
| 90min | 21.0°C | 0.0 | 32.0 | 0 | 0.3 | 0 | 32.3°C |

Notice how P drives the initial response, D dampens the approach (preventing overshoot at 20min), and I slowly corrects the remaining offset once the room is within the deadband.

---

## Control Modes: Continuous vs PWM

SAT supports two fundamentally different ways of controlling the boiler. Understanding when each mode is appropriate is key to getting good results.

### Continuous mode (default)

In continuous mode, SAT sends the PID output directly as a flow temperature setpoint via `CS=xx.x`. The boiler modulates its burner to reach and maintain this temperature. This is the preferred mode for modern condensing boilers that can modulate down to low output levels.

**Flow-temperature-aware clamping**: When the PID output drops quickly (e.g., room warms up), the boiler water is still hot. Sending a sudden low setpoint would cause the boiler to stop and restart — exactly the short-cycling we want to avoid. Continuous mode clamps the minimum setpoint to `boiler_temp - 5°C`, allowing a gradual ramp-down.

```
  Flow temp
  50°C ┤───────╲
       │        ╲  PID says 30°C, but boiler is at 48°C
  45°C ┤         ╲    clamp: min = 48 - 5 = 43°C
       │          ╲
  40°C ┤           ╲──── gradual ramp down
       │               ╲
  35°C ┤                ╲───── eventually reaches PID target
       │                     ╲
  30°C ┤──────────────────────╲────── PID target
       ├────┬────┬────┬────┬────┬──
       0   30   60   90  120  150  seconds
```

### PWM mode

Some boilers cannot modulate well at low output levels — they have a minimum modulation of 30-40%, which is too much for mild weather. PWM mode solves this by cycling the flame on and off at a calculated duty cycle.

**How it works**:

1. PID output is converted to a duty cycle: `duty = (pid_output - base_offset) / (max_setpoint - base_offset)`
2. Within each control interval, the flame is ON for `duty * interval` and OFF for the rest
3. When ON, the full PID setpoint is sent; when OFF, `CS=10` (minimum) suppresses the flame
4. A minimum flame-on time of 30 seconds prevents excessively short burner runs

```
  Example: duty = 0.6, interval = 30s → 18s ON, 12s OFF

  Setpoint
  45°C ┤  ┌──────────────────┐            ┌──────────────────┐
       │  │  flame ON (18s)  │            │  flame ON (18s)  │
       │  │  CS=45.0         │            │  CS=45.0         │
  10°C ┤──┘                  └────────────┘                  └────
       │     flame OFF (12s)                flame OFF (12s)
       │     CS=10.0                        CS=10.0
       ├────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬──
       0    5   10   15   20   25   30   35   40   45   50   55  60s
```

**Edge cases**: Duty >= 95% sends continuous full setpoint. Duty <= 5% sends continuous minimum.

### When to use which mode

| Situation | Recommended mode |
|-----------|-----------------|
| Modern condensing boiler with good modulation | Continuous |
| Boiler with high minimum modulation (>30%) | PWM |
| Underfloor heating (slow response) | Continuous |
| Radiators with oversized boiler | PWM |
| Not sure | Start with Continuous, enable auto-switch |

---

## Cycle Tracker and Auto-Switch

The cycle tracker monitors every flame ON/OFF transition, measures what happened during each cycle, classifies it, and uses this history to automatically switch between continuous and PWM modes.

### Cycle lifecycle

```
  Flame ON                              Flame OFF
  ────────►                             ────────►
     │                                     │
     ▼                                     ▼
  Record start time                    Calculate duration
  Reset max/min flow temp              Classify cycle
  Record setpoint at start             Store in history buffer
  Start sampling                       Update counters
     │                                     │
     ▼                                     │
  Every loop iteration:                    │
  - Track max flow temp                    │
  - Track overshoot seconds                │
  (flow > setpoint + 3°C margin)           │
```

### Cycle classification

Each completed cycle is classified into one of five categories:

| Class | Condition | Meaning |
|-------|-----------|---------|
| **GOOD** | Duration >= 120s, flow reached setpoint, minimal overshoot | Boiler modulated correctly |
| **OVERSHOOT** | Max flow > setpoint + 3°C for > 10s | Boiler overshot — too much heat |
| **UNDERHEAT** | Max flow never reached setpoint - 2°C | Boiler couldn't reach setpoint |
| **SHORT** | Duration < 60s | Flame cycled too fast |
| **UNCERTAIN** | Duration 60-120s | Not enough data to judge |

### Auto-switch logic

When `bPwmAutoSwitch` is enabled (default), SAT monitors sustained conditions and switches modes automatically:

```
  ┌─────────────┐    sustained overshoot    ┌───────────┐
  │  CONTINUOUS  │    for > 60 seconds       │    PWM    │
  │    mode      ├──────────────────────────►│   mode    │
  │              │                           │           │
  │              │◄──────────────────────────┤           │
  │              │    sustained underheat    │           │
  │              │    for > 180 seconds      │           │
  │              │                           │           │
  │              │◄──────────────────────────┤           │
  └──────────────┘    PWM saturation         └───────────┘
                      (flame on > 300s
                       continuously)
```

**Sustained overshoot** (continuous → PWM): If the flow temperature stays more than 3°C above the setpoint for 60 continuous seconds, the boiler cannot modulate down enough. SAT switches to PWM to artificially create off-time.

**Sustained underheat** (PWM → continuous): If the flow temperature stays more than 2°C below the setpoint for 180 continuous seconds during PWM mode, the duty cycling is preventing the boiler from reaching the desired output. SAT switches back to continuous.

**PWM saturation** (PWM → continuous): If the flame has been ON continuously for more than 300 seconds during PWM mode, the duty cycle is effectively 100% and PWM is adding no value. SAT switches to continuous.

### Cycle history buffer

The tracker maintains a circular buffer of the last 8 cycles. This is used for diagnostic reporting (visible via REST API and Web UI) and could be extended in the future for more sophisticated auto-tuning decisions.

---

## Safety System

Safety is the most important aspect of any heating controller. SAT implements six independent defense layers following a defense-in-depth strategy. The universal safe state is `CS=0` — which releases the control setpoint override and lets the wall thermostat control the boiler directly.

### Design principles

1. **Fail safe**: Any failure sends `CS=0`. The boiler reverts to thermostat control.
2. **RAM only**: All safety state is transient. No flash writes needed for safety. This means safety cannot be corrupted by flash failures.
3. **Manual recovery**: Once safety trips, SAT stays disabled until the user explicitly re-enables it. No automatic retry that could mask a persistent problem.
4. **Independent layers**: Each layer operates independently. A bug in one layer doesn't compromise the others.

### The six defense layers

```
  ┌──────────────────────────────────────────────────────────┐
  │ Layer 1: Boot Safety                                     │
  │ On every boot/reboot, send CS=0 before SAT starts.       │
  │ If PIC not ready: deferred CS=0 on first available call. │
  ├──────────────────────────────────────────────────────────┤
  │ Layer 2: Disable Safety                                  │
  │ When SAT is turned off (setting, MQTT, REST):            │
  │ satDisable() → CS=0 + reset all state + PID reset.       │
  ├──────────────────────────────────────────────────────────┤
  │ Layer 3: Stale Sensor Fallback                           │
  │ External indoor temp: expires after 5 minutes.           │
  │ External outdoor temp: expires after 10 minutes.         │
  │ Falls back to OT bus values silently.                    │
  ├──────────────────────────────────────────────────────────┤
  │ Layer 4: Hard Temperature Ceiling                        │
  │ Underfloor: max 50°C (prevents pipe/screed damage).      │
  │ Radiator: max 80°C (boiler physical limit).              │
  │ Minimum: 10°C (prevents freeze, allows flame-off).       │
  │ Enforced EVERY control cycle, after all calculations.    │
  ├──────────────────────────────────────────────────────────┤
  │ Layer 5: Consecutive Failure Counter                     │
  │ If room temp is invalid 10 times in a row → safety trip. │
  │ Any valid reading resets the counter to zero.            │
  ├──────────────────────────────────────────────────────────┤
  │ Layer 6: PIC Communication Check                         │
  │ If PIC is unavailable 5 cycles in a row → safety trip.   │
  │ Any successful CS= command resets the counter.           │
  └──────────────────────────────────────────────────────────┘
```

### Boot safety in detail

After a crash, watchdog reset, or power cycle, the PIC gateway chip retains the last `CS=` value it received. Without boot safety, the boiler could continue running at the last setpoint indefinitely — even if the ESP takes minutes to fully boot.

```
  Power loss          ESP boot            PIC ready         SAT control
  ────┤                 ├──────────────────┤──── ─ ─ ─ ─ ─ ─┤
      │                 │                  │                  │
      │  PIC holds      │  initSAT():     │  Deferred CS=0  │  First
      │  last CS=45     │  try CS=0       │  if missed in   │  computed
      │  from before    │  ↓              │  init           │  CS=xx.x
      │  the crash      │  PIC ready?     │                  │
      │                 │  YES → send CS=0│                  │
      │                 │  NO → set flag  │                  │
      │                 │  for deferred   │                  │
      │                 │  send           │                  │
```

### Safety trip and recovery

When any safety layer trips:

1. `state.sat.bSafetyTripped` is set to `true`
2. `satDisable()` is called: sends `CS=0`, resets PID, resets mode to OFF
3. The control loop returns immediately on every subsequent call
4. The `safety_tripped` field is visible in REST API, MQTT, and Web UI (red badge)

**Recovery**: The user must explicitly re-enable SAT (via settings, MQTT `sat/enabled`, or REST). The `satHandleEnabled(true)` function clears `bSafetyTripped` and resets all failure counters.

### Boiler status evaluator

SAT tracks the boiler's behavior using a 15-state finite state machine. This is not directly a safety mechanism, but it provides diagnostic visibility into what the boiler is doing:

| Status | Description |
|--------|-------------|
| `OFF` | No flame, initial state |
| `IDLE` | No flame, waiting for demand |
| `PREHEATING` | Flame just ignited, boiler far below setpoint |
| `IGNITION_SURGE` | Flame just ignited, boiler already near setpoint |
| `HEATING` | Flame on, boiler temp rising toward setpoint |
| `MODULATING_UP` | Flame on, modulation increasing |
| `MODULATING_DOWN` | Flame on, modulation decreasing |
| `AT_SETPOINT` | Flame on, boiler temp within ±3°C of setpoint |
| `OVERSHOOT_COOLING` | Boiler temp above setpoint + 2°C |
| `POST_CYCLE` | Flame recently turned off after a heating cycle |
| `COOLING` | Flame just turned off |
| `STALLED_IGNITION` | (reserved) Flame expected but not detected |
| `ANTI_CYCLING` | (reserved) Boiler in anti-cycling delay |
| `PUMP_STARTING` | (reserved) Pump running before flame ignition |
| `WAITING_FLAME` | (reserved) Flame requested, waiting for ignition |

The status is derived from flame state, modulation level, and boiler temperature every control cycle. It is published via MQTT and shown in the Web UI.

---

## Integration Guide

### Configuration settings

SAT is configured through the standard OTGW settings system. Settings persist to LittleFS flash and survive reboots.

| Setting | Default | Range | Description |
|---------|---------|-------|-------------|
| `satenabled` | `false` | bool | Master enable/disable |
| `satsystem` | `0` | 0-1 | 0 = radiator, 1 = underfloor |
| `sattargettemp` | `20.0` | 5-30 °C | Desired room temperature |
| `satcoefficient` | `1.5` | 0.1-5.0 | Heating curve steepness |
| `satdeadband` | `0.25` | 0.05-2.0 °C | PID deadband width |
| `satinterval` | `30` | 10-300 s | Control loop interval |
| `satexternaltemp` | `false` | bool | Use external indoor sensor |
| `satpresetcomfort` | `21.0` | 15-28 °C | Comfort preset |
| `satpreseteco` | `18.0` | 10-22 °C | Eco preset |
| `satpresetaway` | `15.0` | 5-18 °C | Away preset |
| `satpwmautoswitch` | `true` | bool | Auto-switch PWM/continuous |

Settings can be changed via the Web UI settings page, the REST API (`POST /api/v2/settings`), or MQTT.

### REST API

Full documentation: [docs/api/README.md](../api/README.md)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/v2/sat/status` | GET | Full runtime state (JSON) |
| `/api/v2/sat/target` | POST | Set target room temperature |
| `/api/v2/sat/externaltemp` | POST | Push external indoor temperature |
| `/api/v2/sat/externaloutdoor` | POST | Push external outdoor temperature |

POST endpoints accept both raw values (`21.5`) and JSON bodies (`{"value":"21.5"}`). Invalid or non-numeric input returns HTTP 400.

**Quick start**:

```bash
# Enable SAT
curl -X POST -d '{"name":"satenabled","value":"true"}' \
  http://otgw.local/api/v2/settings

# Set target temperature
curl -X POST -d '{"value":"21.0"}' \
  http://otgw.local/api/v2/sat/target

# Check status
curl http://otgw.local/api/v2/sat/status
```

### MQTT

Full documentation: [docs/api/MQTT.md](../api/MQTT.md)

**Published topics** (under `OTGW/value/<node-id>/`):

| Topic | Retained | Description |
|-------|----------|-------------|
| `sat/mode` | yes | `off`, `continuous`, or `pwm` |
| `sat/setpoint` | yes | Final flow temperature sent to boiler |
| `sat/heating_curve` | yes | Heating curve base value |
| `sat/pid_output` | yes | PID corrected output |
| `sat/target` | yes | Current target room temperature |
| `sat/error` | no | PID error (target - room) |
| `sat/pid_p` | no | Proportional term |
| `sat/pid_i` | no | Integral term |
| `sat/pid_d` | no | Derivative term |
| `sat/boiler_status` | no | Boiler status code (0-14) |
| `sat/cycle_class` | no | Last cycle classification (0-5) |
| `sat/pwm_duty` | no | PWM duty cycle (0.00-1.00) |
| `sat/safety_tripped` | no | Safety shutdown active |

**Subscribe topics** (under `OTGW/set/<node-id>/sat/`):

| Topic | Payload | Description |
|-------|---------|-------------|
| `target` | `"21.5"` | Set target temperature (persisted) |
| `indoor_temp` | `"20.8"` | Push external indoor reading (5min expiry) |
| `outdoor_temp` | `"8.0"` | Push external outdoor reading (10min expiry) |
| `enabled` | `"true"` / `"false"` | Enable/disable SAT (persisted) |
| `control_mode` | `"continuous"` / `"pwm"` / `"auto"` | Set control mode |

### Home Assistant auto-discovery

When MQTT is enabled, SAT entities are automatically discovered by Home Assistant:

| Entity | Type | Description |
|--------|------|-------------|
| Climate | `climate.otgw_sat` | Full climate entity with target temp and mode |
| Sensor | `sensor.otgw_sat_setpoint` | Flow temperature setpoint |
| Sensor | `sensor.otgw_sat_heating_curve` | Heating curve value |
| Sensor | `sensor.otgw_sat_pid_output` | PID output |
| Sensor | `sensor.otgw_sat_error` | PID error |
| Sensor | `sensor.otgw_sat_mode` | Control mode |
| Sensor | `sensor.otgw_sat_boiler_status` | Boiler status |
| Sensor | `sensor.otgw_sat_pwm_duty` | PWM duty cycle |

The climate entity supports setting the target temperature from the HA dashboard, which is persisted to the ESP's flash.

### Web UI

The SAT tab in the Web UI provides a real-time dashboard:

- **Status badge**: Shows Disabled / Active (Continuous) / Active (PWM) / Safety Tripped
- **Temperature cards**: Room temp, outside temp, target temp, final setpoint
- **PID details**: Error, P/I/D terms, Kp/Ki/Kd gains
- **Cycle info**: Cycle count, last classification, max flow temp
- **ECharts graph**: Temperature history (room, outside, setpoint, heating curve) with up to 720 data points

The dashboard is visualization-only — it polls `/api/v2/sat/status` every 5 seconds. All control logic runs on the ESP independently.

### Using external sensors

SAT can use temperature readings from external sources (e.g., a Zigbee room sensor or a weather station) instead of the OT bus values. This is useful when:

- Your thermostat doesn't report room temperature on the OT bus
- You want more accurate readings from a sensor placed in a specific room
- You want to use a separate outdoor weather sensor

**Setup with Home Assistant automation**:

```yaml
automation:
  - alias: "Push room temperature to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.living_room_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/indoor_temp"
          payload: "{{ states('sensor.living_room_temperature') }}"

  - alias: "Push outdoor temperature to OTGW SAT"
    trigger:
      - platform: state
        entity_id: sensor.outdoor_temperature
    action:
      - service: mqtt.publish
        data:
          topic: "OTGW/set/otgw-AABBCCDDEEFF/sat/outdoor_temp"
          payload: "{{ states('sensor.outdoor_temperature') }}"
```

Remember to enable `satexternaltemp` in settings for indoor readings, and to push values at least every 5 minutes (indoor) or 10 minutes (outdoor) to prevent staleness fallback.

---

## Quick Start Guide

1. **Enable SAT**: Set `satenabled = true` in Web UI settings or via REST/MQTT
2. **Choose heating system**: Set `satsystem` to `0` (radiator) or `1` (underfloor)
3. **Set coefficient**: Start with the suggested value from the coefficient table. Adjust up if rooms don't reach target temperature, down if they overshoot.
4. **Set target temperature**: Either in settings (`sattargettemp`) or via the HA climate entity
5. **Monitor**: Watch the SAT dashboard in the Web UI or HA sensors. The heating curve value and PID terms will show you how the controller is behaving.
6. **Fine-tune**: After a few days, adjust the coefficient up or down based on results. The deadband can be narrowed (e.g., 0.15°C) for tighter control or widened (e.g., 0.5°C) for less frequent corrections.

### Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Room never reaches target | Coefficient too low | Increase by 0.2-0.3 |
| Room overshoots target | Coefficient too high | Decrease by 0.2-0.3 |
| Frequent boiler cycling | Boiler can't modulate low enough | Enable PWM auto-switch or force PWM mode |
| Safety trips on startup | PIC not ready when SAT initializes | Normal — deferred CS=0 handles this, re-enable SAT |
| "External temp stale" in logs | Sensor push stopped | Check HA automation / MQTT connectivity |
| High I term (>5°C) | Sustained offset, possible sensor issue | Check room temp accuracy, verify coefficient |

---

## References

- **SAT upstream**: [github.com/Alexwijn/SAT](https://github.com/Alexwijn/SAT) — Original Home Assistant integration
- **OpenTherm Protocol Specification v4.2**: `docs/opentherm specification/` in this repository
- **ADR-062**: `docs/adr/ADR-062-sat-smart-autotune-thermostat-integration.md` — Architecture decision record
- **REST API docs**: `docs/api/README.md` — Full endpoint reference
- **MQTT docs**: `docs/api/MQTT.md` — Topic reference
- **OTGW firmware**: [otgw.tclcode.com](https://otgw.tclcode.com/firmware.html) — PIC gateway documentation
