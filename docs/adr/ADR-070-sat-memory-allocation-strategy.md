# ADR-070: SAT Memory Allocation Strategy

**Status:** Accepted
**Date:** 2026-04-09

## Context

The SAT (Smart Autotune Thermostat) implementation adds a heating controller to the firmware,
introducing new state variables, lookup tables, control algorithm state, and weather forecast
buffers. The ESP8266 has approximately 40 KB of usable DRAM and a 4 KB cooperative CONT stack.
Every allocation decision in SAT code directly affects firmware stability.

The broader project has established that heap allocation is forbidden (ADR-004, superseded by
ADR-053) and the String class is banned in hot paths (ADR-049). SAT must follow these rules and
additionally must not overflow the cooperative stack, which is easy to violate with large local
arrays or deeply nested calls.

Key constraints for SAT specifically:

- The SAT control loop runs on a timer (every 30-600 seconds), but MQTT publish can run after
  every OT frame and during auto-configure. Both paths must avoid conflicting access to shared
  buffers.
- SAT contains a 24-element `float` forecast array (~96 bytes), a cycle history ring buffer,
  and PID state — none of which can live on the stack without risking overflow.
- `SATcontrol.ino`, `SATpid.ino`, `SATcycles.ino`, and `SATweather.ino` are separate
  translation units (via Arduino `.ino` concatenation). Cross-file data access requires
  accessor functions, not direct global variable sharing.
- PROGMEM must be used for all constant tables (manufacturer table, string constants) to avoid
  consuming DRAM with read-only data.

### Alternatives considered

1. **Local stack variables for algorithm state** — Rejected: PID state (`_pid_integral`,
   `_pid_rawDerivative`, etc.) must survive across calls. Making them local would lose state
   between control cycles. Stack depth in the control loop already exceeds 200 bytes; adding
   large arrays would risk overflow.
2. **Heap allocation for large SAT buffers** — Rejected: violates ADR-053. A `new float[24]`
   for the forecast array would fragment the heap at an unpredictable time and add nullable
   pointer complexity.
3. **Global struct for all SAT state** — Partially used: `state.sat.*` in `OTGWState`
   (ADR-051) holds the public runtime state that MQTT publish and REST API need. Private
   algorithm state (PID internals, cycle phase tracking, weather forecast buffer) is kept
   as file-scope statics in each `.ino` file to enforce encapsulation.

## Decision

**All SAT algorithm state uses static file-scope variables (never heap, never large stack
locals, never String class).** Constant tables are stored in PROGMEM. The String class is
permitted only in `SATweather.ino`'s HTTP fetch path, as documented below.

### Allocation categories

**1. Public runtime state: `state.sat.*` (global struct, ADR-051)**

Fields that MQTT publish, REST API, and the Web UI need are placed in the `OTGWState`
struct (`state.sat.*`). These are zero-initialized at boot and reset when SAT is disabled.
Examples: `state.sat.fFinalSetpoint`, `state.sat.bActive`, `state.sat.eBoilerStatus`,
`state.sat.fPidP/I/D`.

**2. Private algorithm state: file-scope `static` variables**

Each SAT module owns its private state as file-scope statics with a `_module_` prefix:

| Module | Variables | Purpose |
|--------|-----------|---------|
| `SATpid.ino` | `_pid_integral`, `_pid_rawDerivative`, `_pid_lastRoomTemp`, etc. | PID state, survives between cycles |
| `SATcycles.ino` | `_cycle_flameOn`, `_cycle_maxFlowTemp`, `_ema_dutyRatio`, etc. | Cycle tracker state and EMAs |
| `SATcontrol.ino` | `_sat_consecutiveSkips`, `_thermal_coeffEma`, etc. | Safety counters, thermal learning |
| `SATweather.ino` | `_weather_forecastTemp[24]`, `_weather_forecastCount` | 24-hour forecast buffer |

**3. Constant tables: PROGMEM**

The manufacturer lookup table (17 entries × ~14 bytes each ≈ 238 bytes) is stored in PROGMEM:

```cpp
static const SATMfrEntry satManufacturerTable[] PROGMEM = { ... };
```

Access uses `pgm_read_byte()` and `strncpy_P()`. No DRAM consumed for this table.

**4. String class exception: weather HTTP fetch**

`SATweather.ino` uses `String payload = http.getString()` to receive the Open-Meteo JSON
response. This is the only permitted String usage in SAT code. Rationale: the HTTP fetch is
a one-off, low-frequency operation (default every 15 minutes), not a hot path. The payload
is parsed and discarded before the next yield. The String is never held across a control
cycle or MQTT publish. This exception matches the ADR-004 / ADR-049 allowance for
"one-off setup/init contexts."

**5. Formatting buffers: local with explicit bounds**

Short-lived formatting buffers (`char valBuf[16]`, `char cmdBuf[16]`, etc.) are kept as
local stack variables with explicitly bounded sizes. These are safe because:
- All are under 32 bytes
- They never survive across a `yield()` or `feedWatchDog()` call
- They are used with `snprintf_P` / `dtostrf` — never unbounded

**6. No cross-file direct variable access**

Private file-scope statics are accessed only via accessor functions:
```cpp
// SATcycles.ino exposes:
float satCycleGetDutyRatio();
const char* satCycleGetPhaseName();
SATCycleClass satCycleGetLastClass();
```

This prevents accidental writes from `SATcontrol.ino` and avoids extern declarations that
would make the variables effectively global.

## Consequences

### Benefits

- No heap allocation in SAT — zero fragmentation risk from SAT code
- Full BSS footprint of SAT static state is visible at link time (~450 bytes: forecast
  buffer 96 bytes + cycle history ~120 bytes + PID state ~60 bytes + control state ~80 bytes
  + BLE state ~80 bytes)
- PROGMEM manufacturer table saves ~238 bytes of DRAM
- File-scope statics enforce module encapsulation without requiring a C++ class hierarchy
- One-off String in weather fetch does not affect control path stability

### Trade-offs

- `_module_prefix` convention is a discipline requirement; the compiler will not enforce it
- The `state.sat.*` / file-scope split means SAT state is spread across two locations; callers
  must know which fields are in `state.sat` vs. which are internal
- Forecast buffer is always resident (96 bytes BSS) even when weather is disabled

### Risks

- If `SATweather.ino` HTTP fetch is called more frequently (e.g., due to timer misconfiguration),
  the String allocation recurs repeatedly. The timer guard (`DUE(timerWeatherPoll)`) and the
  minimum poll interval (`WEATHER_POLL_MIN_SEC = 300`) prevent this in practice.

## Related

- ADR-004: Static Buffer Allocation Strategy (original, superseded)
- ADR-053: Large Feature Buffer Static Allocation (supersedes ADR-004; all-static rule)
- ADR-049: String Prohibition in Protocol Paths (hot-path String ban, weather exception)
- ADR-051: Dual Encapsulating Structs (settings/state architecture, `state.sat.*`)
- ADR-062: SAT Integration (overall SAT architecture, source file split)
- `SATcontrol.ino`, `SATpid.ino`, `SATcycles.ino`, `SATweather.ino`: implementation
- `OTGW-firmware.h`: `OTGWState.sat` struct definition
