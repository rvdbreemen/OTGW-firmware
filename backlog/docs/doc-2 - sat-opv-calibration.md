---
id: doc-2
title: sat-opv-calibration
type: other
created_date: '2026-04-09 05:22'
---
# SAT OPV Calibration Procedure

OPV stands for Overshoot Protection Value. It is the maximum flow temperature your boiler can actually achieve at minimum modulation (MM=0). SAT uses this value to switch from continuous modulation to PWM mode before an overshoot can occur, rather than reacting after the fact.

---

## What OPV Is and Why It Matters

When a boiler runs at minimum modulation (MM=0), it still produces heat at a base output level. If the heat demand is low, the boiler can overshoot the target flow temperature. The OPV is the ceiling — the highest flow temperature the boiler reaches under these conditions, typically measured over a 20-minute window.

Without OPV:
- SAT uses time-based cycle tracking and EMA overshoot fraction to decide when to switch modes.
- Mode switching is reactive (it waits for an overshoot to happen).

With OPV calibrated and enabled:
- SAT switches to PWM mode proactively, before the setpoint is exceeded by more than the overshoot margin.
- This results in fewer overshoot cycles and better temperature stability.

---

## Prerequisites

Before starting calibration:

1. **SAT must be enabled** — go to the SAT dashboard or set `sat/enabled = true` via MQTT.
2. **Boiler must be operational** — flame must be achievable. Do not calibrate during summer or when heat demand is zero.
3. **Heating demand should be present** — run the calibration when outside temperatures require heating (typically below 12°C outdoor).
4. **Allow at least 30 minutes** — the full calibration sequence takes up to 30 minutes.
5. **Do not manually change setpoints** during calibration — the procedure sends its own CS= and MM= commands.

---

## How to Start Calibration

### Via MQTT

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_start" -m "1"
```

### Via REST API

```bash
curl -X POST http://otgw.local/api/v2/sat/settings/ovp_start
```

### Via WebUI

On the SAT Settings page, scroll to the OPV section and click **Start OPV Calibration**.

---

## Calibration Phases

The calibration runs as a state machine. You can monitor progress via:
- `sat/ovp_calib_phase` field in the REST API status (`GET /api/v2/sat/status`)
- The OPV Calibration field in the SAT dashboard

| Phase | Name | Description |
|-------|------|-------------|
| 0 | `idle` | No calibration running |
| 1 | `starting` | Sends CS=max and MM=0 to boiler |
| 2 | `warming` | Waits for flame and flow temp to rise at least 5°C above start temp |
| 3 | `measuring` | Samples boiler temperature every 10 s for 20 minutes, tracks peak |
| 4 | `done` | Calibration successful — OPV value saved, boiler returned to normal |
| 5 | `failed` | Calibration failed — see Troubleshooting section |
| 6 | `cooldown` | (internal, transitions back to idle) |

---

## Step-by-Step Procedure

1. Confirm SAT is enabled and boiler is operational.
2. Start calibration via MQTT, REST, or WebUI (see above).
3. The firmware sends `CS=<max_setpoint>` and `MM=0` to the boiler, then waits for the flame to ignite and the flow temperature to rise at least 5°C above its starting value (warming phase). This can take 1-3 minutes.
4. Once warm, the firmware enters the measuring phase. It samples boiler temperature every 10 seconds for 20 minutes, tracking the peak value. During this phase it continues to send `MM=0` to maintain minimum modulation.
5. After 20 minutes, the firmware:
   - Saves the peak temperature as `settings.sat.fOvpValue`
   - Sets `settings.sat.bOvpEnabled = true`
   - Sends `CS=0` and `MM=<configured max>` to return the boiler to normal operation
6. Calibration is complete. The OPV value is persisted to flash and takes effect immediately.

---

## Expected Output

After a successful calibration:
- `sat/ovp_value` MQTT topic will publish the measured maximum flow temperature (°C), retained.
- `sat/ovp_enabled` will be `"true"`, retained.
- `sat/ovp_calib_phase` in the REST API status will return `0` (idle).
- The telnet debug output will show: `OPV: calibration DONE! OPV=52.5 from 120 samples`

A typical OPV value for a condensing gas boiler with radiators at minimum modulation is 45–60°C.

---

## How to Stop / Cancel

To cancel a running calibration:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_stop" -m "1"
```

Or via REST:

```bash
curl -X POST http://otgw.local/api/v2/sat/settings/ovp_stop
```

After cancellation the firmware sends `CS=0` and `MM=<max>` to restore normal operation.

---

## Setting OPV Manually

If you already know your boiler's minimum-modulation peak temperature (from boiler documentation or prior measurements), you can set it directly without running a calibration:

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_value" -m "52.5"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_enabled" -m "true"
```

---

## Disabling OPV

To disable OPV (revert to reactive mode switching):

```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/sat/ovp_enabled" -m "false"
```

SAT will still switch modes, but based on the sustained overshoot timer (60 seconds above setpoint + overshoot margin) rather than the calibrated value.

---

## Troubleshooting

### Phase stays at `warming` for more than 3 minutes
**Cause**: Flame did not ignite within the timeout window (3 minutes).
**Result**: Calibration transitions to `failed`.
**Fix**: Ensure there is heating demand. Check that the boiler is not in summer mode or lockout. Retry after confirming manual heating works.

### Calibration times out entirely (30 minutes)
**Cause**: A phase exceeded the 30-minute total timeout.
**Result**: Calibration transitions to `failed`.
**Fix**: Check OTGW serial communication. If PIC is not responding to CS= or MM= commands, calibration cannot proceed. Check `otgw-pic/picavailable` MQTT topic.

### OPV value seems too low (e.g., 35°C)
**Cause**: Calibration ran when heating demand was low, the boiler throttled itself, or outdoor temperature was near the heating curve knee point.
**Fix**: Re-run calibration in colder weather or raise the maximum CH setpoint temporarily.

### OPV value seems too high (e.g., 75°C)
**Cause**: The boiler's modulation did not actually drop to minimum during calibration, or the boiler has very high minimum output.
**Fix**: Verify the MM=0 command was effective by checking `sat/boiler_status` and modulation level during calibration. Some boilers ignore low MM= values.

### Boiler stays at high setpoint after calibration failure
**Cause**: The cleanup phase (CS=0 and MM=max) did not complete.
**Fix**: Send commands manually:
```bash
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/command" -m "CS=0"
mosquitto_pub -h <broker> -t "OTGW/set/otgw-AABBCCDDEEFF/command" -m "MM=100"
```

---

## Re-Calibration

Re-run calibration when:
- Boiler is replaced or serviced.
- Firmware is upgraded to a new major version (OPV behavior may change).
- Observed behavior suggests OPV is no longer accurate (repeated overshoots despite OPV enabled).

The calibration overwrites the previous OPV value. There is no undo — if needed, set the old value manually as described above.

---

## Source Reference

- Calibration state machine: `src/OTGW-firmware/SATcontrol.ino`, function `satOvpCalibrate()` (~line 234)
- Start/stop functions: `satOvpStartCalibration()` / `satOvpStopCalibration()` (~lines 308–325)
- Timing constants:
  - Total timeout: 30 minutes (`SAT_CALIB_TIMEOUT_MS`)
  - Flame wait: 3 minutes (`SAT_CALIB_FLAME_WAIT_MS`)
  - Measurement window: 20 minutes (`SAT_CALIB_MEASURE_MS`)
  - Sample interval: 10 seconds (`SAT_CALIB_SAMPLE_MS`)
  - Warm-up delta: 5°C (`SAT_CALIB_WARM_DELTA`)
