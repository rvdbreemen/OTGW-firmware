# Capture upload format (otgw_capture.py, ADR-202)

## Files

| File | What it holds |
|---|---|
| `summary.txt` | Run settings and results: device host, broker typed, LongRun, telnet gaps, MQTT message count, frame count, firmware, PIC, stop reason |
| `gaps.log` | One line per stream gap: start, end, seconds, stream, reason |
| `index.csv` | Long-run: per hour and stream the lines, bytes, first/last time, gaps |
| `hours/YYYYMMDD-HH.zip` | Long-run: one zip per wall-clock hour with the files below |
| `otframes.log` | Raw OpenTherm frames: `host_time <TAB> device_time <TAB> source <TAB> 8 hex digits`; `#` lines are notes (connects, disconnects) |
| `otframes.csv` | Same frames decoded: host_time, device_time, source, frame, msgid, type, value_hb, value_lb, f88, u16 |
| `otgw_simulation.log` | Replay file for `/api/v2/simulate` |
| `telnet.log` | Debug console: OT decode lines, MQTT sends (`Sending MQTT: server <broker>`), MQTT commands received (`Message arrived on topic [...] = [CMD]`), PIC command queue (`CmdQueue:`), SAT ticks |
| `mqtt.log` | Everything the broker published under the topic filter (empty when the broker address was wrong) |
| `snapshots.log` | Every 5 min: `/api/v2/device/info` and `/api/v2/sat?detail=full`, secrets redacted (free heap, uptime, SAT state) |
| `crashlog.log`, `curl-probes.log`, `browser.log` | Crash log polls, HTTP probes, web UI console |
| `transcript-*.txt` | Non-long-run: everything merged into one file |

The frame log writes a frame one frame late: the gap between consecutive device times is the
frame gap shifted by one frame (memory `reference_otlog_timestamp_skew`).

## Source letters

| Letter | Meaning |
|---|---|
| T | Request from the thermostat / master |
| B | Answer from the boiler / heat pump (slave) |
| R | Request the gateway (PIC) sent to the slave in place of, or changed from, the master's |
| A | Answer the gateway sent to the master in place of the slave's |
| E | Error frame |

Many R/A frames mean the PIC overrides something: a changed MsgID 0 (for example DHW enable
cleared by HW=0), a TSP read (TP=) inserted in place of a master read, a CS override.

## Frame decode

8 hex digits = 32 bits: bit 31 parity, bits 30-28 message type, bits 23-16 MsgID, bits 15-0
data (HB, LB). Types: 0 Read-Data, 1 Write-Data, 2 Invalid-Data, 4 Read-Ack, 5 Write-Ack,
6 Data-Invalid, 7 Unknown-DataID. Temperatures are f8.8 (signed, /256).

## MsgID 0 status

Master HB: bit0 CH enable, bit1 DHW enable, bit2 cooling, bit3 OTC, bit4 CH2.
Slave LB: bit0 fault, bit1 CH mode, bit2 DHW mode, bit3 flame (compressor on a heat pump),
bit4 cooling, bit5 CH2, bit6 diagnostic.
Typical heat-pump pattern: LB 2 (CH mode, pump only) for about 2 min, then LB 10 (CH + flame);
LB 4 then 12 for hot water.

## Common MsgIDs

1 control setpoint, 5 fault flags, 9 remote override, 11 TSP, 14 max modulation, 16 room setpoint,
17 modulation level, 18 CH pressure, 24 room temperature, 25 supply (Tboiler), 26 DHW temperature,
27 outside, 28 return, 56 DHW setpoint, 57 max CH setpoint, 100 remote override function.
