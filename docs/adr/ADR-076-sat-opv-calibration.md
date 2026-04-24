# ADR-076: SAT OPV (Optimal Valve Position) Calibration

**Status:** Accepted
**Date:** 2026-04-09

## Context

Many residential boilers have a minimum modulation level above which the burner cannot operate stably. If the SAT controller sends a flow temperature setpoint that is too close to the boiler's minimum output temperature, the boiler may short-cycle (ignite, quickly overshoot, and shut down) rather than modulate smoothly. This is particularly harmful for condensing boilers, where short cycling reduces efficiency and increases wear.

The OPV (Optimal Valve Position) concept, taken from the SAT Python project, represents the maximum boiler flow temperature achievable at minimum modulation (MM=0, i.e., minimum relative modulation level). By observing this temperature during a controlled calibration procedure, the firmware can:

1. Know the boiler's minimum output temperature
2. Use this as a floor for setpoints in continuous mode, preventing the boiler from trying to modulate below its stable range
3. Avoid sending setpoints so low that the boiler short-cycles

### Why calibration is necessary

The boiler's minimum modulation temperature is not available via OpenTherm: the OT protocol provides `MaxCapacity` (MsgID 17), `RelModLevel` (MsgID 17), and `MaxRelModulation` (MsgID 14), but no "minimum stable flow temperature" field. The only way to determine this value for a specific boiler is to measure it empirically.

### Alternatives considered

1. **Use a fixed conservative floor (e.g., 30°C)** — Rejected: too low for some boilers (causes short cycling), too high for others (unnecessarily limits the control range). A calibrated value is installation-specific and always correct.
2. **Derive from MaxCapacity and boiler specs** — Rejected: MaxCapacity is expressed as a percentage of rated output, not as a flow temperature. The mapping from percentage to temperature depends on the boiler's heat exchanger characteristics, which are not in the OT specification.
3. **Ask the user to enter the value manually** — Rejected: most users do not have access to boiler commissioning documentation. Even heating engineers rarely know this value without measurement. A guided in-firmware procedure is more reliable.
4. **Use the boiler's reported setpoint as a proxy** — Rejected: the boiler reports the requested setpoint (what the master sent), not the minimum stable temperature. These are different quantities.
5. **Continuous learning during normal operation** — Rejected: during normal operation, the boiler modulates freely. Isolating the minimum modulation temperature requires deliberately commanding MM=0 while observing the result. Inferring it from normal operation would take weeks of data and would be confounded by varying heat demand.

## Decision

Implement an active OPV calibration procedure as a state machine in `SATcontrol.ino` (`satOvpCalibrate()`), triggered by user request (REST or MQTT command).

### Calibration procedure

The procedure runs as a non-blocking state machine with the following phases:

| Phase | Duration | Action |
|---|---|---|
| `SAT_CALIB_STARTING` | Immediate | Send `CS=<maxSetpoint>` and `MM=0` to force minimum modulation at maximum demand |
| `SAT_CALIB_WARMING` | Up to 3 minutes | Wait for flame ignition and initial temperature rise (≥5°C above starting temp) |
| `SAT_CALIB_MEASURING` | 20 minutes | Sample boiler temperature every 10s, track peak. Keep sending `MM=0` to maintain minimum modulation |
| `SAT_CALIB_DONE` | Immediate | Store peak temperature as `settings.sat.fOvpValue`, enable OPV with `settings.sat.bOvpEnabled = true` |
| `SAT_CALIB_FAILED` | Immediate | Send `CS=0` and `MM=100` to recover normal operation |

**Total maximum duration: 30 minutes** (`SAT_CALIB_TIMEOUT_MS = 1800000ms`).

### What is measured

The peak boiler flow temperature (`Tboiler`) observed during the 20-minute measuring phase, while the boiler runs at minimum modulation (`MM=0`). This peak temperature represents the maximum heat output at minimum burner firing rate.

### How the calibrated value is used

After calibration, when SAT is in continuous mode and would compute a setpoint below `settings.sat.fOvpValue`, the setpoint is raised to `fOvpValue` instead. This prevents the boiler from running below its minimum stable output temperature, which would cause short cycling.

### Safety during calibration

- The procedure sends `CS=<maxSetpoint>` during warming to ensure the boiler fires
- On failure or completion, `CS=0` is sent immediately to release boiler control
- The `MM=100` command is sent on recovery to restore the user's maximum modulation setting
- All commands go through `addCommandToQueue()` as required (ADR-016); no direct serial writes
- The calibration state machine is polled from the SAT control loop, which is already timer-guarded

### Storage

The calibrated value is stored in the persistent settings struct (`settings.sat.fOvpValue`) and saved to LittleFS. It survives reboots. The `settings.sat.bOvpEnabled` flag controls whether the floor is applied; it is set to true automatically when calibration completes successfully.

### Re-calibration

A new calibration procedure can be started at any time via REST/MQTT. The previous `fOvpValue` is overwritten when the new measurement completes. Calibration can be disabled entirely by clearing `bOvpEnabled` via the settings API without re-running the procedure.

## Consequences

### Benefits

- Calibration is fully automated: no boiler documentation or manual measurements required
- The procedure is installation-specific: every boiler gets the correct floor for its own characteristics
- Failure handling is safe: `CS=0` is sent on timeout, ensuring the boiler is not left at an artificially high setpoint
- The value persists across reboots: calibration is a one-time setup step

### Trade-offs

- The 20-minute measuring phase is disruptive: during calibration, the boiler runs at maximum setpoint and minimum modulation, which is not normal heating operation. Users should run calibration when the home is unoccupied or heat demand is low
- The peak temperature during minimum modulation may vary with supply gas pressure and ambient temperature. A calibration done in summer may not perfectly represent winter minimum output

### Risks

- If the boiler does not fire during the warming phase (e.g., due to anti-cycling lockout or DHW demand), the calibration times out and fails. The 3-minute wait for flame may be too short for boilers with long ignition delays
- `MM=0` on some boilers may mean "use boiler's own minimum" rather than "fire at absolute minimum burner rate". The actual behavior is boiler-firmware-dependent

## Related

- ADR-085: SAT Smart Autotune Thermostat Integration (overall context)
- ADR-016: OpenTherm Command Queue (all boiler commands via queue)
- ADR-051: Dual Encapsulating Structs (settings persistence)
- `SATcontrol.ino`: `satOvpCalibrate()`, `satOvpStartCalibration()`, `satOvpStopCalibration()`
- `OTGW-firmware.h`: `SATCalibPhase` enum, `settings.sat.fOvpValue`, `settings.sat.bOvpEnabled`
- SAT upstream: https://github.com/Alexwijn/SAT (OVP concept)
