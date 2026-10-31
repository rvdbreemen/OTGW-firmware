# Bench check: scripted on-device verification

`scripts/bench_check.py` (Python stdlib only) runs a list of checks from a JSON file against a
bench unit and writes a timestamped transcript as evidence. Use one checks file per task, run it
after the OTA flash, attach the transcript to the task's Final Summary.

```
python scripts/bench_check.py --host 192.168.88.61 --checks docs/guides/bench-check-example.json --out <dir>
```

- Exit 0 = every check passed, 1 = at least one failed, 2 = bad checks file.
- Transcript: `<out>/bench-check-<host>-<YYYYMMDD-HHMMSS>.txt` (every request, response excerpt,
  matched telnet lines, a verdict per check) and a last `SUMMARY` line (also printed).
- It only reads and presses the keys you list. It never flashes or reboots.

## Device rules it follows

- One HTTP request in flight, 0.4 s gap between requests (REST gate is capped at 2).
- A 503 or a connection error is retried once after 1.5 s; the retry is logged.
- Telnet: reconnects when silent for 8 s (a dead first connection after boot). When the device
  closes the connection at once (client cap reached, e.g. a capture is attached) it backs off 3 s
  per attempt. Detach the capture, or accept that the telnet check may fail for that reason.

## Checks file

`{"checks": [ ... ]}` (or a bare list). Every check has `type` and an optional `name`.

| type | fields |
|---|---|
| `version` | `regex` matched against `/api/v2/device/info` `device.fwversion` (proves the flashed build runs) |
| `rest` | `path`, `method` (GET default), `body` (POST JSON), `expect_code` (200), `json` (rules), `regex` (raw text) |
| `telnet` | `timeout` s (30), `keys`, `restore_toggles`, `all_of` (every regex seen), `any_of` (at least one), `regex` + `count_min` (lines matching >= n, default 1) |
| `wait` | `seconds` (plain sleep), or `path` + `json` + `timeout` + `interval` (poll REST until the rules hold) |

JSON rules: `{"path": "a.b.c", "equals"|"gt"|"lt"|"contains"|"regex"|"exists": value}`. Dotted paths
tolerate keys that contain dots: `/api/v2/debug` keys are flat, so the SAT flag is
`debug.state.debug.sat`.

Telnet debug keys are toggles. With `"restore_toggles": true` the same `keys` are pressed again
at the end, so the device returns to its original debug state. A telnet check stops as soon as
all its conditions hold.

## Example

`docs/guides/bench-check-example.json`: version regex, `GET /api/v2/health` is 200, debug field
`state.debug.sat` is true, and `SATTICK` appears at least twice within 70 s on telnet.

## Limits

Checks only what the REST API and the telnet debug log show. A telnet match is a regex on text,
not proof of the control outcome: pick lines that carry the value you want to prove.

## While a capture is running

The device accepts one telnet client, and a running capture holds it. Pass `--telnet-log <capture run folder>` (or a `telnet.log` file): telnet checks then read the lines the capture writes (they follow the hourly rotation of the long-run mode). Key presses are not possible through a log; a check with `keys` fails with a clear message.

```
python scripts/bench_check.py --host 192.168.88.61 --checks docs/guides/bench-check-example.json --telnet-log %LOCALAPPDATA%/OTGW-capture/<run>/<stamp>
```
