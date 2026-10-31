# Scripts Directory

This directory contains utility scripts for the OTGW-firmware project.

## autoinc-semver.py

Version management script that automatically increments the build number and updates version information in `version.h`.

### Usage

```bash
python scripts/autoinc-semver.py . --filename version.h --githash <hash>
```

### Options

- `directory` - Directory containing version.h
- `--filename` - Version file name (default: version.h)
- `--git` - Enable git integration (auto-commit)
- `--increment-build` - Amount to increment build number (default: 1)
- `--githash` - Override git hash
- `--githash-length` - Length of short git hash (default: 7)
- `--prerelease` - Override prerelease identifier
- `--update-all` - Update version strings in all project files (automatically enabled when version mismatch detected)

### What it does

1. Parses `version.h` to extract version components
2. Increments the build number
3. Updates timestamp and git hash in `version.h`
4. Generates semantic version strings (_SEMVER_FULL, _SEMVER_CORE, etc.) in `version.h`
5. Updates `data/version.hash` with the git hash
6. **Automatically detects version mismatches** between `version.h` and project files (`.h`, `.ino`, `.cpp`, `.c`)
7. Updates all project files with correct version when mismatch detected or `--update-all` flag is used
8. Optionally commits changes to git

### Automatic Version Propagation

The script now automatically detects when the semantic version (MAJOR.MINOR.PATCH-PRERELEASE) in `version.h` differs from version strings in project files. When a mismatch is detected, all project files are automatically updated to match `version.h`.

**Note**: Only semantic version changes trigger automatic updates. Build number changes do not trigger updates since build numbers are not included in project file version strings.

**Excluded directories**: The script skips the following directories when scanning/updating:

- `build`, `scripts`, `data`, `.github`, `docs`, `hardware`, `example-api`
- `arduino`, `Arduino`, `libraries`, `staging`
- `node_modules`, `.git`, `__pycache__`
- `Specification`, `specification`, `theme`
- Any hidden directories (starting with `.`)

This script is used both by the CI/CD workflow and the local build script.

## branch-hygiene-queue.ps1

Generates a branch hygiene review queue for remote branches and exports it as CSV.

### Branch Hygiene Usage

```bash
pwsh -File scripts/branch-hygiene-queue.ps1 -Remote origin -BaseBranch dev -InactiveDays 14 -OutputCsv docs/process/branch-hygiene-queue.csv
```

### Branch Hygiene Options

- `-Remote` - Remote name to inspect (default: `origin`)
- `-BaseBranch` - Base branch used for merge classification (default: `dev`)
- `-InactiveDays` - Inactivity threshold for stale-unmerged classification (default: `14`)
- `-OutputCsv` - Path to output CSV file

### Branch Hygiene Behavior

1. Fetches and prunes remote refs
2. Enumerates remote branches excluding `origin/HEAD`, `origin/main`, and `origin/dev`
3. Classifies each branch as `active`, `stale-merged`, or `stale-unmerged`
4. Adds owner/decision/notes columns for manual review
5. Exports a sorted review queue CSV for branch governance

## capture-mqtt-debug (.bat / .sh) and otgw_capture.py

One diagnostic capture tool for every platform (ADR-202, TASK-1475). The
capture itself is `otgw_capture.py` (standard-library Python 3.9+). Thin
launchers find a Python and run it:

- `capture-mqtt-debug.bat` (Windows) tries `py -3`, `python` and `python3`. When
  none is 3.9 or newer it downloads the portable Python 3.12.10 from python.org
  once, checks its SHA-256 and unpacks it into `%LOCALAPPDATA%\OTGW-capture\`.
- `capture-mqtt-debug.sh` (Linux, macOS) tries `python3` and `python`. When none is 3.9
  or newer it uses a portable CPython 3.12.16 (python-build-standalone, release
  20261026) from an earlier run, or downloads it once (about 65 MB), checks its
  SHA-256 and unpacks it into `~/.local/share/otgw-capture/` (Linux, or
  `$XDG_DATA_HOME/otgw-capture/`) or `~/Library/Application Support/otgw-capture/`
  (macOS). Needs `curl` or `wget`, and `sha256sum` or `shasum`. Linux with musl
  libc (Alpine) and CPU types other than x86_64, aarch64 and armv7 (Linux) or
  x86_64 and arm64 (macOS) are not covered: install Python 3.9+ there.
  `capture-mqtt-debug-macos.sh` is a compatibility wrapper around it.

MQTT uses paho-mqtt 2.1.0, imported straight from
`vendor/paho_mqtt-2.1.0-py3-none-any.whl`. No `mosquitto_sub` is needed and
nothing is installed. Without the wheel, the MQTT stream is skipped and the
other sources still run. The flash zip ships the launchers, `otgw_capture.py`
and `vendor/`; keep them together.

### Usage

```bash
scripts/capture-mqtt-debug.sh
scripts/capture-mqtt-debug.sh --device 192.168.1.50 --broker 192.168.1.10 --duration 120
scripts/capture-mqtt-debug.sh --device 192.168.1.50 --broker ""          # no MQTT stream
```

```bat
scripts\capture-mqtt-debug.bat
scripts\capture-mqtt-debug.bat -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10 -DurationSeconds 120
```

Both spellings work on every platform (`--device` or `-DeviceHost`,
`--duration` or `-DurationSeconds`, and so on). Missing values are prompted for
only when stdin is a terminal; `--no-prompt` / `-NoPrompt` never prompts. An
empty broker skips the MQTT stream and writes one summary line saying so
(TASK-1453). Press `Q` or Ctrl+C to stop; the transcript is still written.

### Options

- `--device` / `-DeviceHost`: OTGW device IP address or hostname
- `--broker` / `-BrokerHost`: MQTT broker; empty skips the MQTT stream
- `--broker-port` / `-BrokerPort`: MQTT broker port (default `1883`)
- `--topic` / `-Topic`: MQTT topic filter (default `#`)
- `--username` / `--user` / `-Username`, `--password` / `-Password`: MQTT credentials
- `--save-secrets` / `-SaveSecrets`: also store the MQTT password in the settings file (for unattended runs)
- `--duration` / `-DurationSeconds`: stop after N seconds (`259200` = 3 days; above 2 hours the long-run mode starts by itself)
- `--long-run` / `--hourly` / `-LongRun`: long-run mode, see "3-day capture" below
- `--snapshot-seconds` / `-SnapshotSeconds`: device snapshot interval (default 300 in long-run mode, off otherwise, `0` = off)
- `--min-free-gb` / `-MinFreeGb`: long-run mode stops cleanly below this much free disk space (default `1.0`)
- `--output-root` / `-OutputRoot`: output folder root (default `logs/mqtt-diagnostics`)
- `--no-prompt` / `-NoPrompt`: never prompt
- `--telnet-connect-timeout-seconds`, `--telnet-reconnect-delay-ms`, `--telnet-post-disconnect-delay-ms`, `--telnet-idle-check-seconds` (and the `-Telnet...` names): telnet reconnect tuning (TASK-1299)
- `--skip-browser-capture`, `--browser-url`, `--browser-debug-port`, `--browser-path`: headless Edge/Chrome/Chromium devtools capture
- `--skip-crashlog-capture`, `--crashlog-url`, `--crashlog-poll-seconds`: crash log and `reboot_log.txt` polling
- `--skip-frame-capture`: do not record the raw OpenTherm frames (`otframes.log`, `otgw_simulation.log`)
- `--skip-http-probes`, `--http-probe-timeout-seconds`: post-capture HTTP probes

`-MosquittoSubPath` and `-SkipToolInstall` are still accepted and ignored.

### Saved settings

The last-used device, broker, port, topic and username pre-fill the next run's
prompts. They are stored in `%LOCALAPPDATA%\OTGW-capture\capture-settings.json`
(Windows) or `~/.config/otgw-capture/capture-settings.json` (macOS, Linux); the
environment variable `OTGW_CAPTURE_SETTINGS` overrides the path. The MQTT
password is stored only with `--save-secrets`. Keys that other tools added
(WifiSsid, Hardware, ...) are kept. `scripts/_secrets.py` reads the same file.

### What it collects

- **Telnet debug (port 23).** It reconnects with a gap report and a TCP keepalive, and a health-probe watchdog checks a silent link (TASK-1299). On each connection it switches on every debug toggle that is off, and simulator toggles (`SensorSim`, `OTGW-Sim`) are left alone. At stop it restores every toggle to its state at capture start (TASK-1453). The `q` / `D` settings dump is sent only after `Setup finished!` was seen or the uptime is above 0 minutes (TASK-1396).
- **MQTT.** One `topic payload` line per message in `mqtt.log`, the same format as `mosquitto_sub -v` (in long-run mode with time stamp and retain flag, see below).
- **Browser devtools.** Console, exceptions, log entries and network timing of the device page in `browser.log`. On Windows (job object) and Linux (parent-death signal), the headless browser also dies when the tool is killed.
- **Crash log.** `/api/v2/device/crashlog` and `/reboot_log.txt`, polled into `crashlog.log`.
- **Raw OpenTherm frames (TASK-1503).** One WebSocket client on the device's `/ws` (the stream the web UI's OT log uses) records every frame into two files that stay in the run folder next to the transcript (they are not merged into it and not deleted):
  - `otframes.log`: a `#` header line, then one line per frame, tab separated: host receive time (ISO-8601, milliseconds, local time with UTC offset), the device timestamp (`HH:MM:SS.uuuuuu`, device local time), the source letter (`T` thermostat, `B` boiler, `R` request to the boiler, `A` answer to the thermostat, `E` parity error) and the 8 hex digits. Lines starting with `#` are notes (connect, disconnect). The device logs a frame one frame late, so both times belong to the arrival of the next frame; the difference between two consecutive lines is the frame gap, shifted by one frame.
  - `otgw_simulation.log`: the same frames in exactly the replay format, `T10010A00` per line, LF endings. No frame is filtered; the letters are kept so a replay tool can drop frames by letter.
  The stream is on by default on PIC and OT-Direct boards, so no setting is changed. It is one extra client of the 3 the device allows: with the headless browser capture also running, the user's own browser still gets a slot, a second browser tab may be refused. Under heap pressure the device may drop log lines (`canSendWebSocket()`), so a gap in the file is not proof of a gap on the bus. If `/ws` cannot be opened, the recorder retries with a growing delay and logs the reason in `script.error.log`; the rest of the capture continues. The summary lists the files and the frame count.
- **HTTP probes.** At stop, `/`, `/index.html`, `/v2.js` and key REST endpoints with status, timing and size. Bodies are cut at 200 characters, except `/api/v2/health` and `/api/v2/device/info`, which are kept whole up to 8 KB so the `hd_min_*` heap fields survive (TASK-1396).

Everything except the two frame files is merged into
`transcript-<date-time>-<firmware-version>-<hostname>-<uniqueid>.txt`, under
`logs/mqtt-diagnostics/<yyyyMMdd-HHmmss>/`. Secrets (password/token/key values,
`-P` arguments) are redacted. After a successful merge, the intermediate files
are removed. At the end the tool also writes ONE upload file next to the run folder, `<run folder>-upload.zip`, and prints
its path as the last line (`Upload this one file: ...`). It holds the merged transcript, the two frame files and the CSV files
converted from the logs (`csv/sattick.csv` and the other SAT record types from the telnet log, `csv/otframes.csv`). Upload that
one file; keep `otframes.log` and `otgw_simulation.log` in the run folder for replays.

### 3-day capture (long-run mode, TASK-1523)

For a run of many hours or days, for example the field data for the SAT replay work:

```bash
scripts/capture-mqtt-debug.sh --device 192.168.1.50 --broker 192.168.1.10 --long-run --duration 259200
```

```bat
scripts\capture-mqtt-debug.bat -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10 -LongRun -DurationSeconds 259200
```

`--long-run` is switched on by itself for any `--duration` above 2 hours (7200 s); the tool then prints a
`LONG-RUN MODE IS ACTIVE` line with the path of the single upload zip. Leave `--duration` out to run until you press `Q`. What changes compared with a short run:

- **One segment per wall-clock hour.** Telnet, MQTT, raw frames (`otframes.log`, `otgw_simulation.log`), browser, crash log and snapshots write into `live/YYYYMMDD-HH/`. At each full hour the finished hour is packed into one zip, `hours/YYYYMMDD-HH.zip`. The plain files of that hour are deleted only after the zip was written and read back (`testzip`); if packing fails they are kept and packed again later. The live hour stays plain text, so you can `tail` it. On stop, the last partial hour is zipped too. A line that arrives at the hour change lands in the old or the new hour, never in neither.
- **CSV files in every hour zip.** Before an hour is zipped, its logs are converted and the CSVs go into the same zip: `sattick.csv`, `satboot.csv`, `satmode.csv`, `sattc.csv`, `satfollow.csv`, `satdhw.csv` (SAT records of the telnet log, via `sat_log_to_csv.py`; kinds without lines are left out), `otframes.csv` (columns `host_time, device_time, source, frame, msgid, type, value_hb, value_lb, f88, u16`; `type` is the OpenTherm message type such as `READ-DATA` or `WRITE-ACK`, `f88` the signed 8.8 reading of the value, `u16` the raw value; note and gap lines are skipped) and `mqtt.csv` (`host_time, retain, topic, payload`). A conversion error is logged in `script.error.log` and the raw logs are zipped anyway. A line is never split over two hours.
- **`index.csv`** instead of one merged multi-GB transcript: per hour and stream the number of lines, bytes, first and last time and the number of gaps. A small `transcript-*.txt` stays in the run folder; it holds the summary, errors, `gaps.log`, the index and the last 300 lines per stream of the final hour.
- **One file to upload.** At stop the tool writes `<run folder>-upload.zip` next to the run folder and prints `Upload this one file: <path>` as its last line. It holds all `hours/*.zip`, `index.csv`, `gaps.log`, `summary.txt` (with the device identity) and the short transcript. It needs as much free disk space again as the hour zips take (they are stored, not compressed twice).
- **MQTT lines** carry a time stamp and the retain flag: `host_time<TAB>retain<TAB>topic<TAB>payload`, one line per message (CR and LF in a payload are escaped). Short runs keep the `mosquitto_sub -v` format.
- **Gaps.** Telnet, MQTT and the `/ws` frame recorder reconnect without end. Each outage leaves a marker with start, end and reason in the stream, one line in `gaps.log` and a count in `index.csv`. A gap in the frame file is not proof of a gap on the OpenTherm bus (the device may drop log lines under heap pressure).
- **Snapshots every 5 minutes** (`snapshots.log`): `GET /api/v2/device/info` (heap and loop-gap counters are in there) and `GET /api/v2/sat?detail=full`, one request at a time. A failed request is one `ERROR` line. The snapshot poller and the crash-log poller share one request slot, so together they never have more than one request in flight.
- **Disk guard.** The start prints an estimate (roughly 400 MB per day of plain text, about 60 MB per day zipped; extrapolated from one 45-minute capture, not measured over days). Free space is checked every 5 minutes, and the run stops cleanly, with everything packed, below `--min-free-gb` (default 1 GB).

Practical notes: keep the PC awake (a sleeping PC records nothing and the gap shows up only as a long pause), keep the output folder on a drive with a few GB free, and prefer `--skip-browser-capture` when you only need the data streams, because the headless page polls the device for the whole run. Upload the one `<run folder>-upload.zip` file. Hour zips are written to a temporary file, read back, flushed to disk and only then moved into place; an hour whose plain files could not be deleted afterwards is never zipped a second time. If the PC clock jumps back, writes stay in the newest open hour.

#### Replaying a capture on a bench unit

Not on a live heating system (see "Simulation" in `docs/api/README.md`). The firmware replay reads `/otgw_simulation.log` from LittleFS, one frame every 750 ms, and ignores the recorded timing:

1. Upload the file under that name: `curl -F "upload=@otgw_simulation.log" http://<ip>/upload` (the `/upload` handler, field `upload`; add `-u` credentials when an admin password is set). It replaces an earlier copy.
2. Start it with `POST /api/v2/simulate/start`, check it with `GET /api/v2/simulate`, stop it with `POST /api/v2/simulate/stop`. `source` is `file` while the uploaded file is used.
3. Delete the file from LittleFS afterwards to return to the built-in fixture.

A replay that honours the timing in `otframes.log` is a separate follow-up.

Tests: `python tests/test_capture_tool.py` runs the tool against fake telnet,
HTTP and MQTT servers, and the long-run segment store with an injected clock.

## sat_boiler_emulator.py

Host-side synthetic boiler emulator for OTGW32 bench testing (TASK-802). Connects to
the OTDirect TCP bridge on port 25238 and periodically sends synthetic OpenTherm frames
in OTGW monitor-stream format so the TASK-795 simulation availability gate
(satOnBoilerDetected / SAT simulation contract §4.2) can be exercised without a
physical boiler wired to the OT bus.

Requires Python 3 only (standard library: socket, argparse, time, sys). No pip deps.

### Quick start

```powershell
# Dry-run: print computed frames and exit (no socket opened)
python scripts\sat_boiler_emulator.py --dry-run

# Dry-run with specific member-id and slave config flags
python scripts\sat_boiler_emulator.py --dry-run --member-id 4 --slave-config 0x01

# Connect and emit frames every second
python scripts\sat_boiler_emulator.py --host 192.168.1.x

# Connect with custom interval and member-id
python scripts\sat_boiler_emulator.py --host 192.168.1.x --port 25238 --member-id 4 --interval 2.0
```

### Options

| Option | Default | Description |
|---|---|---|
| `--host HOST` | (required unless `--dry-run`) | OTGW32 hostname or IP address |
| `--port PORT` | `25238` | TCP port of the OTDirect bridge |
| `--member-id N` | `4` | Slave MemberID code (0-255; 0=customer non-specific) |
| `--slave-config FLAGS` | `0x00` | Slave configuration flags byte for MsgID 3 HB (decimal/hex/binary) |
| `--interval SEC` | `1.0` | Seconds between frame bursts |
| `--dry-run` | off | Print frames and exit without connecting |

### Frames emitted

**MsgID 3 - Slave Configuration / MemberID (READ-ACK)**

The key frame for the §4.2 availability gate: `otDirectBoilerPresent()` in
`OTDirect.ino:292-295` returns `otBoilerCacheValid[3]`, which is set in
`handleMasterResponse()` when a real boiler ACKs MsgID 3.

Frame layout (32 bits, MSB first):

```
bit31   bits30-28   bits27-24   bits23-16   bits15-8           bits7-0
Parity  MsgType     Spare       Data-ID     HB (slave cfg)     LB (MemberID)
  0     100(=4)     0000        0x03        0x00               member_id
```

For `--member-id 4 --slave-config 0x00`: wire line `B40030004`
For `--member-id 4 --slave-config 0x01` (DHW present): wire line `BC0030104`

**MsgID 0 - Status (READ-ACK, all-zero slave status)**

Wire line `BC0000000` (healthy idle boiler: no fault, no flame, no active CH/DHW).

### Port 25238 input/output asymmetry - bench validation required

The OTGW32 port 25238 bridge is bidirectional but asymmetric:

- **Output** (firmware to client): monitor-stream lines with `B`/`T`/`R`/`A` prefixes
- **Input** (client to firmware): PIC-style commands (`XX=value` format)

A raw `B40030004` line sent as INPUT does not match the `XX=value` parser, so it will
NOT populate `otBoilerCacheValid[3]` via the TCP path.

Because the TCP bridge cannot inject a boiler response, **AC#2/#3 of TASK-795 are
exercised through the firmware test hook** instead (TASK-802 path A):

```powershell
# assert a synthetic boiler is present -> trips the §4.2 availability gate
python scripts\sat_boiler_emulator.py --host 192.168.1.x --rest-force-boiler on
# expect: simulation auto-disables within ~1s; POST /api/v2/sat/mode (enable) -> 409;
#         MQTT sat enable command rejected
python scripts\sat_boiler_emulator.py --host 192.168.1.x --rest-force-boiler off
```

`POST /api/v2/sat/force-boiler` (body `0|1`) is a test-only, transient override
(cleared on reboot) that `satBoilerHardwarePresent()` honours; turning it on also
trips the deferred boiler-detected edge so the "boiler appeared -> simulation off"
teardown runs exactly as a real boiler frame would. Trusted-LAN only.

The frame-builder / `--dry-run` / TCP modes remain useful as:

1. A reference for what a correctly-formed slave-response frame looks like.
2. A TCP connection harness so bench state can be observed over the monitor stream.
3. A starting point for adapting to an OT-bus hardware slave device.
