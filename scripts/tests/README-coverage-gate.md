# OT decode-coverage regression gate (2.0.0)

Turns the OT replay fixture into a pass/fail gate: a run is reduced to a
normalized fingerprint and compared against a committed baseline taken from a
known-good firmware. Ported from `otgw-1.x.x` (TASK-1070); every difference below
is forced by 2.0.0 behaviour rather than chosen.

```
# one command: upload, start, capture, stop, compare.
# exit 0 = match, 1 = drift, 2 = device or broker error.
OTGW_MQTT_PASSWORD=... python run_coverage_test.py --host <ip> --http-password <pw>

# refresh the baseline, only after reading a diff and accepting the new behaviour
... python run_coverage_test.py --host <ip> --http-password <pw> --record
```

## Assets

| File | What it is |
|---|---|
| `otgw_simulation_coverage.log` | The fixture: 423 frames, 376 distinct, covering every id in `OTmap[]` plus out-of-map ids, all six message types, all five source prefixes including the `E` parity path. One loop is 317 s at the default 750 ms pacing. |
| `make_simulation_coverage.py` | Regenerates the fixture after `OTmap[]` gains ids. |
| `coverage_baseline.py` | Fingerprint, diff, record, selftest. |
| `mqtt_topic_capture.py` | Stdlib MQTT 3.1.1 subscriber; also runs standalone to list what a device publishes. |
| `baseline_coverage.json` | The committed baseline. |
| `run_coverage_test.py` | The one-command runner. |

Capture logs are NOT committed. `.gitignore` allowlists the two fixture `.log`
files by name rather than un-ignoring `scripts/tests/*.log` wholesale, so a gate
run cannot accidentally add a megabyte of capture to a commit.

## Two transports, one window

The fingerprint has two halves and they cannot come from the same place.

**Decoded frames** are read from the debug telnet with **MQTT debug off**. With it
on, a MsgID that fans out to ~9 topics emits its publish lines in about 10 ms,
overruns the telnet, and drops whatever follows. The tell is a truncated line
with the next timestamp spliced onto it:

```
21:14:56.141905 ... sendMQTTData(1402): ... => To21:14:56.881179 (  43620| 31732) logMQTTValue(1678): MQT
```

That hole swallowed `AC0630000`'s decode line while the frame itself was
processed perfectly normally. Measured back-to-back on the bench S3: MQTT debug
**on** gives 24 spliced lines and only 363 of 366 OT keys present in both halves
of a selftest; **off** gives 1 spliced line, 369 of 369 keys in both halves, and
all 376 distinct fixture frames each seen at least twice.

**Topics** are read from an MQTT subscription, not from those publish log lines.
The log is lossy in the same way, so a topic's presence in it is a sample: two
identical runs differed by 9 topics, with the smaller set a strict subset of the
larger (loss is subtractive, never invention) while payloads differed on 0 of the
215 shared topics. A broker sees every publish, so presence becomes an
observation and can be gated.

Both work over **one** window, because `MQTTDebugTf` gates only the log line and
never the publish: the device can keep MQTT debug off while the broker still sees
everything. The subscriber runs on a thread alongside the telnet capture.

`--topics telnet` falls back to reading topics from the debug log. Presence is
then reported but not gated (see below).

## What fails the gate, and what is only reported

| Section | MISSING / NEW | CHANGED |
|---|---|---|
| `ot`, `msgtypes`, `sources` | **fails** | **fails** |
| `mqtt`, from the broker | **fails** | **fails** |
| `mqtt`, from telnet | informational | **fails** |

The fingerprint records `mqtt_source`, and `mqtt` presence is gated only when
baseline and run both came from the broker. Mixing them is refused by
construction: a broker baseline compared against a telnet run would report every
dropped log line as a regression.

Taking the intersection of several telnet runs instead was considered and
rejected: each run drops a different random subset, so the intersection erodes
with every run rather than converging on a stable core.

## Things the subscriber has to get right

Each of these silently corrupts a baseline rather than failing, so they are
handled explicitly and are worth knowing about:

- **Scope to the device under test.** A broker normally carries more than one
  OTGW; this bench sees `otgw-AC276ECE45D8` (under test) next to
  `otgw-2CF43257D77C` (production). `normalise_topic()` strips the uniqueid to
  keep baselines portable between benches, so unscoped collection merges both
  devices onto the same topic names.
- **Discard the retained flush.** Subscribing replays every retained message
  immediately (31 to 2012 of them here): discovery configs and state left by
  earlier runs or other firmware. Those are not observations of this run, and
  MQTT 3.1.1 has no way to decline them (retain handling arrived in MQTT 5), so
  the subscriber drains and discards for a few seconds before the simulation
  starts.
- **Discard the pre-window backlog too.** The subscription goes live at
  `open_reader()`, but the fixture upload, the simulation start and the preflight
  all happen after that, and the broker keeps publishing throughout. TCP buffers
  it, so the collector's first reads return messages from before the window
  unless the socket is flushed immediately before collection starts (65 of them
  in a measured run). This is not cosmetic: it put an `OFF` for
  `boiler_connected`, published while the device sat idle between runs, into a
  capture whose simulation had been running the entire time, and the gate
  correctly called that a payload change.
- **Take the broker from the device, not from local config.**
  `_secrets.resolve_broker()` returns a stored bench value that goes stale: it
  pointed at an unreachable `192.168.1.11` while the device was publishing
  happily to `homeassistant.local`. The runner reads broker, port, user and top
  topic from `/api/v2/settings` and the MAC from `/api/v2/device/info`, and
  raises rather than falling back if either is missing.
- **A wildcard top topic is not a prefix.** `#` is only legal as the final
  character of a filter, so formatting `<root>/#` yields the illegal `#/#` when
  the stored root is itself `#` (which it is, on this bench). A broker answers an
  illegal filter by closing the connection with no error packet, surfacing much
  later as a bare "broker closed the connection". `topic_filter_for()` handles it.
- **Unique client id per run.** A broker kicks the existing session when a second
  client connects with the same identifier, so a fixed id lets two gate runs, or
  a leftover session from a crashed one, disconnect each other mid-capture.

Credentials come from `scripts/_secrets.py`: `MqttPassword` in the out-of-repo
`capture-settings.json` (written by `capture-mqtt-debug.bat -SaveSecrets`) or
`$OTGW_MQTT_PASSWORD`. Nothing is read from or written to the repository.

## Toggles are driven by echo, never by the banner

The debug keys are toggles, not setters: pressing one that is already on turns it
OFF. The key mapping is also not stable across branches (1.x gives `4` to
MQTTGate; 2.0.0 gives `4` to Sensors and `g` to MQTTGate).

Two sources of truth were tried and rejected:

- **The telnet banner.** Live decode output splices into it before the toggle
  menu prints, so the menu is often absent: the banner ends mid-separator as
  `-----21:21:52.079893 (...`. This failed silently and produced a capture with
  no publish lines, which reads as a regression rather than a broken capture.
- **The device's `D` dump.** It reports every flag by name, which would be
  branch-agnostic, but it is ~290 lines and the busy telnet truncates it: 8 KB in
  12 s and the `[state.debug]` section never arrived.

What works is the per-toggle echo, one short line whose text is identical on both
branches:

```
Debug MQTT: true
Debug MQTT Gating: false
```

`set_debug_flag()` presses, reads the echo back, re-presses if it landed the
wrong way, and treats a missing echo as "wrong key for this branch" only after a
second, balancing press. A flag that cannot be driven raises, so a broken capture
fails at second 0 rather than looking like a regression 12 minutes later.

## Minimum capture length

Two full fixture loops plus a margin (694 s), and the runner refuses to run
shorter. One loop is not enough for two independent reasons:

- A capture cut mid-loop reports topics as MISSING purely because they had not
  come round yet.
- A frame appearing once per loop gets no second chance when the telnet stream
  interleaves and mangles its only occurrence. `T10620000` was lost in loop 1 and
  recovered in loop 2 exactly this way.

Both look identical to a real regression.

## What the fingerprint keeps and strips

Keeps, because it characterises firmware behaviour:

- for each `(prefix, msgtype, msgid)`: the decoded label and rendered value
- for each MQTT topic: the **set** of payloads published
- which message types and source prefixes were exercised at all

Strips, because it varies per run or per bench: timestamps, heap and max-block
columns, task ids, uptime, device uniqueid/MAC, broker host, topic root, and all
non-ASCII (captures carry cp1252 degree signs, so the same firmware would
otherwise fingerprint differently depending on how the log was decoded).

Dropped entirely: `otgw-pic/*` and `otgw-firmware/*`, driven by PIC polling on its
own timer and by device metadata rather than by the fixture. Whether they land
inside a capture window is timing, not decode behaviour.

Also dropped: any line whose decoded value is empty after normalisation, and any
"topic" containing whitespace. Both are unambiguous tells that interleaved output
spliced into the line.

## Before recording a baseline

Each of these was learned by getting it wrong first.

- **Prove stability, do not assume it.** `python coverage_baseline.py selftest
  <capture>` fingerprints two halves of one capture and diffs them. Any drift is
  a volatile field that normalisation failed to strip.
- **Never record across a boot.** A boot brings a discovery drip and `[force]`
  publishes a steady-state run never reproduces. The first 1.x baseline was taken
  across a boot and made every healthy run FAIL with four MISSING `vh_*` topics.
  Grep the capture for a version banner or reset text first.
- **Never record from a run you have not explained.** A capture missing a frame
  the firmware handled correctly bakes that absence in, and the next run reports
  it as NEW. If a diff is 1-2 frames, find out whether they were spliced before
  touching the baseline. Refreshing is not a fix for a flaky capture.
- **Check the device is not a second frame source.** `settings.sat.bSimulation`
  drives synthetic OT traffic asynchronously and is persisted, so it survives
  reboots. It must be off. The live PIC is already handled: `picSerialFlushRx`
  discards live RX during replay, confirmed by `R00000000` appearing exactly once
  per loop and no more.
