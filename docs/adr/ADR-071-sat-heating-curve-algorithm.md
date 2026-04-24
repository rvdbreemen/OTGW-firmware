# ADR-071: SAT Heating Curve Algorithm

**Status:** Accepted
**Date:** 2026-04-09

## Context

The SAT heating controller must determine a target boiler flow temperature given an outdoor
temperature and a desired indoor room temperature. This is the fundamental weather-compensation
problem: as outdoor temperature drops, the boiler must supply hotter water to maintain a
comfortable indoor temperature.

Three broad algorithmic approaches exist:

1. **Fixed setpoint**: the boiler always targets the same flow temperature. Simple but
   inefficient: too hot in mild weather (wasted energy, short cycling), too cold in freezing
   weather (comfort failure).

2. **Linear weather compensation**: `flow = a + b * (target - outdoor)`. One linear slope
   (`b`) calibrated per installation. Common in basic boiler controllers. Fast to compute,
   but a single slope cannot capture the non-linear thermal behavior of buildings (heat loss
   is approximately proportional to the temperature difference but the radiation losses from
   radiators are non-linear with flow temperature).

3. **Non-linear polynomial curve**: a formula that introduces a square term on the outdoor
   temperature deviation, producing a curve that matches the physics of both convective
   and radiative heat transfer more accurately than a linear approximation.

The SAT Python project (https://github.com/Alexwijn/SAT, thermo-nova branch) chose option 3,
derived from the OT-Thing reference implementation and empirically validated against multiple
boiler installations. The formula is:

```
curveValue = 4*(target - 20) + 0.03*(outdoor - 20)^2 - 0.4*(outdoor - 20)
setpoint   = baseOffset + (coeff / 4) * curveValue
```

Where:
- `target` = desired indoor room temperature (degrees C)
- `outdoor` = current outdoor temperature (degrees C)
- `baseOffset` = heating-system-specific constant (27.2 for radiators, 20.0 for underfloor)
- `coeff` = user-configured heating curve coefficient (typically 0.6 - 1.4)
- `20.0` = reference temperature (SAT_HC_REF_TEMP): outdoor temperature where no heating
  is needed at 20°C target

### Why 20°C as the reference?

At an outdoor temperature of 20°C, a building at 20°C target needs negligible heating.
Subtracting 20 from both temperatures centers the polynomial on the no-heating-needed point.
The `0.03 * diff^2` term adds curvature: it contributes zero at 20°C outdoor, grows as the
outdoor temperature drops (more heating needed), and also grows (symmetrically) as outdoor
temperature rises (less relevant but physically consistent).

### Physics motivation for the `0.03 * diff^2` term

A purely linear curve (`flow = constant + slope * (target - outdoor)`) underestimates
required flow temperature at very cold outdoor temperatures and overestimates it at mild
temperatures. The square term corrects for:

- Radiator output is approximately proportional to `(flow - room)^n` with `n` slightly above
  1 for convective radiators. A linear approximation to this is insufficient at large deltas.
- Building heat loss has a small non-linear component due to wind, infiltration, and
  thermal bridges.

The `0.03` coefficient was empirically calibrated in the SAT Python project against real
installations and is not a user-configurable parameter.

### User-configurable coefficient

The `coeff` parameter (default 1.0, range approximately 0.4 - 2.0) scales the entire curve.
A high coefficient makes the curve steeper (suitable for poorly insulated buildings or
high-output radiators). A low coefficient flattens it (well-insulated buildings or large
underfloor heating surfaces). The PID controller (ADR-069) auto-derives its gains from `coeff`,
so adjusting the curve coefficient also re-tunes the PID.

### Alternatives considered

1. **Fixed setpoint (no compensation)** — Rejected: defeats the purpose of SAT. High energy
   waste and comfort problems.

2. **Single linear slope** — Rejected: insufficient accuracy at temperature extremes. The
   `0.03 * diff^2` term is demonstrably needed to match observed boiler behavior at cold
   outdoor temperatures (below -5°C). A linear curve requires re-tuning the slope as seasons
   change; the polynomial does not.

3. **Lookup table (tabular weather compensation)** — Rejected: tables require installation-
   specific calibration and cannot be parameterized by a single `coeff` value. They also
   require more flash/RAM and are harder to tune. The polynomial gives equivalent accuracy
   with a single floating-point coefficient.

4. **Full thermal model (building simulation)** — Rejected: requires multi-day learning,
   extensive commissioning, and significantly more RAM. SAT's thermal learning (Task #21)
   adds a learning layer on top of the fixed curve, but the curve itself must work at
   commissioning without learned data.

5. **SAT Python formula, exact port** — Chosen: the formula is documented, validated
   against multiple installations, and the `coeff` parameter is already understood by SAT
   users. A faithful port avoids introducing new calibration unknowns.

## Decision

Implement the SAT polynomial heating curve exactly as used in the SAT Python thermo-nova
branch, ported to C in `SATcontrol.ino:satCalcHeatingCurve()`:

```cpp
static float satCalcHeatingCurve(float targetTemp, float outsideTemp)
{
  float baseOffset = satGetBaseOffset();  // 27.2 rad / 20.0 underfloor
  float coeff = settings.sat.fHeatingCurveCoeff;
  float diff = outsideTemp - SAT_HC_REF_TEMP;  // SAT_HC_REF_TEMP = 20.0f

  // curveValue = 4*(target - 20) + 0.03*(outside - 20)^2 - 0.4*(outside - 20)
  float curveValue = 4.0f * (targetTemp - SAT_HC_REF_TEMP)
                   + 0.03f * diff * diff
                   - 0.4f * diff;

  float setpoint = baseOffset + (coeff / 4.0f) * curveValue;

  state.sat.fHeatingCurveValue = setpoint;
  return setpoint;
}
```

### Heating-system-specific base offsets

| Heating system | `baseOffset` | Rationale |
|---------------|-------------|-----------|
| Radiators (default) | 27.2°C | Typical radiator system design flow temperature at 20°C indoor / 20°C outdoor |
| Underfloor | 20.0°C | Underfloor systems run at much lower flow temperatures |
| Heat pump | 27.2°C | Uses radiator offset; heat pumps operate at low flow, coeff should be set low |

The base offset represents the design flow temperature at the reference point (20°C indoor,
20°C outdoor): the temperature the boiler must supply even when there is essentially no
heat demand, to account for pipe losses and the thermal inertia of the heating system.

### Output clamping

The heating curve output is clamped before being sent to the boiler:

- Minimum: `SAT_MIN_SETPOINT = 10.0°C` (prevents sending freeze-damage setpoints)
- Maximum: `satGetMaxSetpoint()` — 62°C for radiators, 45°C for underfloor, 40°C for heat pumps
- Hard ceiling: `SAT_HARD_MAX_FLOOR = 50.0°C` / `SAT_HARD_MAX_RAD = 80.0°C` (safety)

The PID output is added to the heating curve value before clamping, so the curve establishes
the coarse setpoint and the PID trims it within the clamped range.

## Consequences

### Benefits

- Single-parameter tuning (`coeff`): users familiar with SAT Home Assistant component can
  transfer their existing coefficient directly
- Accurate at temperature extremes: the square term prevents the under-heating at -10°C
  that a linear curve would produce with the same coefficient
- Auto-gains in PID v3 scale with `coeff`, so curve and controller retune together
- Heating-system-specific offsets and max setpoints prevent underfloor overheating without
  requiring separate per-system coefficients

### Trade-offs

- The `0.03` and `0.4` polynomial coefficients are fixed, not user-configurable. If a
  specific installation has unusual heat loss characteristics, only `coeff` can be adjusted,
  not the curve shape. In practice this has proven sufficient across the SAT user base.
- The formula uses `float` arithmetic, which on ESP8266 is software-emulated (no FPU).
  One curve evaluation is approximately 6 floating-point multiplications and additions —
  negligible at the 30-second control interval, but noted for completeness.

### Risks

- The formula assumes the indoor target temperature range is approximately 15-25°C and
  outdoor temperature range is approximately -20 to +30°C. Extreme values outside these
  ranges may produce unexpected setpoints. The clamp system (min/max) prevents unsafe
  values from reaching the boiler.

## Related

- ADR-085: SAT Integration (overall architecture, module split)
- ADR-069: SAT PID v3 Implementation (PID adds correction on top of heating curve output)
- ADR-070: SAT Memory Allocation Strategy (all curve-related constants are static or PROGMEM)
- `SATcontrol.ino`: `satCalcHeatingCurve()` (line ~343), `satGetBaseOffset()`, `satGetMaxSetpoint()`
- `OTGW-firmware.h`: `state.sat.fHeatingCurveValue`, `settings.sat.fHeatingCurveCoeff`
- SAT upstream: https://github.com/Alexwijn/SAT (thermo-nova branch)
- OT-Thing reference: https://github.com/OTGW32/OT-Thing
