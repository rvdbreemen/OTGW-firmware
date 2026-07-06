# TASK-956: 2-hour representative-load heap-frag soak — partial verdict

Date: 2026-07-06. Device: esp32-classic bench unit, COM8/192.168.88.64,
alpha.331. Driver: `scripts/heap_soak_driver.py`. Raw data:
`build/heap_soak_snapshots.ndjson` (353 snapshots, 20s cadence),
`build/heap_soak_stdout.log` (baseline/final summary).

## This is a PARTIAL result, not the formal multi-day proof

Per prior project convention (long-soak/hardware-gated tasks are not closed
on the strength of a single bench run) and per TASK-956's own procedure
(representative window "e.g. 24-72h"), **this 2-hour run does not by itself
satisfy the proof criterion for a Phase-3 gating-removal decision.** It is
real, clean, on-device evidence — reported honestly as far as it goes, not
stretched into a conclusion the run doesn't support.

## Adaptations from the original procedure (documented, not silent)

- **OT traffic**: the bench unit is `esp32-classic` (PIC), not `esp32-otgw32`
  (OTDirect) — `sat_boiler_emulator.py` requires the OTDirect TCP bridge
  (port 25238), which doesn't exist on this board. Substituted the
  firmware's own "OTGW serial-simulation replay" (telnet `s`,
  `state.debug.bOTGWSimulation`) as the OT-traffic source instead — a real
  firmware feature, not a synthetic stand-in.
- **MQTT discovery republish**: not exercised. No MQTT broker was reachable
  from this environment during the soak window (`homeassistant.local:1883`
  the device is configured for, and the known test-rig broker at
  `192.168.1.234:1883`, are both on networks unreachable from here). This
  soak therefore isolates heap-frag behavior from Web UI + REST + OT traffic
  only, NOT the full MQTT-inclusive profile.
- **Duration**: 2 hours, not the spec's "e.g. 24-72h". A representative
  fragmenting-load profile at this heap size tends to converge quickly (see
  data below), but a short window cannot rule out slow multi-day drift.

## Data

| Metric | Baseline (t=0, post `z` reset) | Min/Max over the 2h run | Final |
|---|---|---|---|
| `freeheap` (current) | 53576 | min 47168 | 53124 |
| `maxfreeblock` (current) | 31732 | held at 31732 throughout | 31732 |
| `hd_min_max_block` (watermark) | 31732 | **min 25588** | 25588 |
| `hd_min_free_heap` (watermark) | 43096 | -- | 30300 |
| `hd_max_loop_gap_ms` | 11 | **max 650** | 650 |
| `hd_enter_low` / `_warning` / `_critical` | 0/0/0 | **0/0/0 the entire run** | 0/0/0 |
| `hd_drip_slowmode` | 0 | **0 the entire run** | 0 |
| `hd_ws_drops` / `hd_mqtt_drops` | 0/0 | **0/0 the entire run** | 0/0 |
| `hd_rest_503` / `hd_webfile_503` | 0/0 | **0/0 the entire run** | 0/0 |

510 representative requests over 2 hours (mixed API polling + occasional
full 4-asset page reload, single-flight, ~20s cadence — see driver docstring
for the exact mix). One transient telnet/HTTP unreachable blip logged (1 of
353 samples), not sustained, did not recur.

## Reading against the proof criterion

- **`min_max_block` stays well above the gating thresholds (8192
  emergency / 1536 promote)**: YES. The watermark settled at 25588 within
  the first ~10 minutes and never moved again for the remaining ~110
  minutes — comfortably above both action thresholds. Note it sits close to
  (just above) the 24000-byte promote-tier boundary in
  `restEffectiveInflightCap()` — not a concern under this load profile, but
  worth knowing the margin isn't huge if load were to intensify.
- **`drip_slowmode`/`mqtt_drops`/`ws_drops`/`enter_low|warning|critical` all
  stay 0**: YES, unambiguously, for the entire 2-hour window.
- **`max_loop_gap_ms` stays low (no multi-second stalls)**: MOSTLY. It rose
  from an 11ms baseline to a 650ms peak. Not itself alarming (two orders of
  magnitude below a "stall"), but a real, nonzero increase worth flagging
  rather than omitting — the cause was not isolated in this pass (could be
  the OT-sim replay's own timing, a periodic housekeeping task, or something
  else; not investigated further here).

## Verdict

**Directionally supportive of "the gating machinery does not engage under
representative load on this ESP32-S3 board," but NOT a complete proof.**
Zero tier-machine engagements and zero drops/503s over 2 hours of
representative traffic is a clean, positive result. It does not cover: (1)
the full 24-72h window the spec asks for, (2) MQTT-inclusive load, (3) real
OT bus traffic via an actual boiler or the OTDirect emulator (only the
onboard serial-sim substitute was available on this PIC board).

**Recommendation: do NOT trigger Phase-3 gating removal on this result
alone.** This is one clean data point in a larger picture the corroborating
KennisBank note on ESP32-S3 fragmentation behavior already gestures toward,
but TASK-956's own criterion was written for a much longer, MQTT-inclusive
window than this environment could produce in one session. A longer soak
(ideally with a reachable MQTT broker and/or on the OTDirect `esp32-otgw32`
target with the real `sat_boiler_emulator.py`) would be needed before
treating the proof criterion as fully met.
