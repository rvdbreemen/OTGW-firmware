# Cloud telemetry: completeness analysis (TASK-1963, 2026-11-15)

Question (Robert): does the telemetry show all firmware data and events, all incoming and outgoing data, and all
sensor data from outside the OpenTherm bus?

Answer: **no.** The PoC cloud recorder (`CloudRecDevice.h`) records three things: OT frames, SATTICK lines and the
text of the `Debug*` macros. The existing local recorder `diagRec` (ADR-213, `diagRecorderStuff.h:76-92`) records
much more as structured events, but it also has gaps: sensors outside the bus, network and connection events,
telnet input, the webhook, OTA, and SAT decisions. Debug text cannot close those gaps: most of it only prints
when a debug flag is on (73 explicit `state.debug.*` guards plus the subsystem macros), and full debug output is
about 3 KB/s against the 0.3 KB/s that fits a board without PSRAM.

## Inventory

Legend: ✓ recorded as a structured event · ~ only indirectly (debug text with a flag on, an MQTT publish, or
derivable from OT frames) · ✗ not recorded

| # | Source / event | Direction | PoC cloud | diagRec | Evidence |
|---|---|---|---|---|---|
| 1 | OT frames T/B/R/A/E with ms time | in | ✓ | ✓ | `OTGW-Core.ino:5965` |
| 2 | PIC commands (queue) | out | ~ | ✓ | `OTGW-Core.ino:4513` |
| 3 | OTDirect commands | out | ~ | ✓ | `OTDirect.ino:3510` |
| 4 | PIC responses (incl. TR/PR answers) | in | ~ | ✓ | `OTGW-Core.ino:6304` |
| 5 | Port 25238 bridge (commands in, stream out) | in/out | ✗ | ✗ (by design) | `OTDirect.ino:980`, `OTGW-Core.ino:6555` |
| 6 | MQTT received | in | ~ | ✓ | `MQTTstuff.ino:821` |
| 7 | MQTT published (value topics) | out | ~ | ✓ | `MQTTstuff.ino:1964`, `:2021` |
| 8 | MQTT HA discovery, connect/disconnect, errors | out/event | ~ | not verified / ✗ | streaming discovery path not checked |
| 9 | REST POST/PUT/PATCH/DELETE (body, secrets left out) | in | ✗ | ✓ | `diagRecorderStuff.ino:88-101` |
| 10 | REST GET, response status of every call | in/out | ✗ | ✗ | GETs are skipped on purpose |
| 11 | File upload/delete (FSexplorer), firmware/FS OTA, safeboot, PIC flash | in/event | ~ | ✗ | `OTGW-Core.ino:7039` (debug text only) |
| 12 | WebSocket connect/disconnect/data | in/event | ~ | ✗ | `webSocketStuff.ino:160-210` (WS data is not used) |
| 13 | Telnet commands (`handleDebugChar`) | in | ✗ | ✗ | `handleDebug.ino:305` |
| 14 | Webhook HTTP call and result | out | ~ | ✗ | `webhook.ino:214` |
| 15 | Setting changes (redacted) | event | ✗ | ✓ | `settingStuff.ino:1412` |
| 16 | Dallas DS18B20 temperatures | sensor | ~ | ~ (only as MQTT publish) | `sensors_ext.ino:443-470` |
| 17 | S0 pulse counter | sensor | ~ | ~ (only as MQTT publish) | `s0PulseCount.ino:54-95` |
| 18 | PIC return/temp sensor (TR poll) | sensor | ~ | ✓ (command + response text) | `OTGW-Core.ino:1221`, `:1399` |
| 19 | BLE thermometers | sensor | ✗ | ✓ | `SATble.ino:835` |
| 20 | Weather (outside temperature, humidity, wind) | sensor | ✗ | ✓ | `SATweather.ino:531` |
| 21 | DHW water meter | derived | ~ | ~ | from ID 19 frames (`OTGW-Core.ino:2920`); the stored total and a reset are not recorded |
| 22 | GPIO outputs | out | ~ | ~ | follow the OT bits (`OTGW-firmware.ino:1229`); a refused pin is not recorded |
| 23 | SAT controller (PID, cycles, pressure health, markers, boiler feed) | event | ~ (SATTICK, only with SAT debug on) | ~ (only its MQTT topics) | `SATcontrol.ino`, `SATpressure.ino:33-62` |
| 24 | WiFi / Ethernet up/down, IP, RSSI | event | ~ | ✗ | `networkStuff.ino:152-165` |
| 25 | NTP time sync | event | ✗ | ✓ | `networkStuff.ino:1401` |
| 26 | Reboot, reset reason, crash summary | event | ✗ | ✓ (at recorder start) | `diagRecorderStuff.h:376` |
| 27 | Heap (free, largest block) | health | ~ (only in the debug line prefix) | ✗ | `debugStuff.ino` (`_debugBOL`) |
| 28 | Task stacks, queue overflows, the recorder's own drops | health | ✗ (drops counted, not in the record) | ✓ (`GAP` only) | `diagRecorderStuff.h:573` |
| 29 | Simulation/replay active (frames not real) | event | ✗ | not verified | |

What is complete: the OpenTherm bus itself (1). With diagRec, the command and response paths (2-4), MQTT values
(6-7), REST writes (9), settings (15), BLE, weather and NTP (19, 20, 25), and reboots (26) are recorded too.

Gaps that matter for "everything":

- **Sensors outside the bus**: Dallas and S0 are only visible as an MQTT publish (so nothing when MQTT is off),
  never as their own measurement.
- **Incoming paths without a record**: REST GET, telnet commands, WebSocket, file uploads, and the port 25238
  bridge.
- **Outgoing without a record**: the webhook, MQTT discovery, and the status code of REST answers.
- **System events**: network up/down, MQTT connection, OTA/safeboot/PIC flash, heap and task health, simulation
  mode.
- **SAT**: no structured decision record; SATTICK only with SAT debug on.

## Proposal

1. **One event source, two sinks.** Make the cloud upload a second sink of diagRec's event stream, not a set of
   hooks next to it. diagRec already has the capture points, secret redaction (`diagNameIsSecret`, the settings
   body) and the drop/`GAP` accounting. The cloud record then carries a diagRec section next to OT (fine timing)
   and an optional debug section. This removes the double hooks in `processOT()`.
2. **New diagRec record types** (a few bytes each, low rate):

   | Type | Payload | Rate |
   |---|---|---|
   | `DALLAS` | index u8, temperature int16 ×100 | per poll per sensor (only on change, plus every 5 min) |
   | `S0` | pulses u16, duration u32 | per send interval |
   | `NET` | link (WiFi/ETH) u8, up/down u8, RSSI int8, IP u32 | per event + RSSI every 5 min |
   | `MQTT_CONN` | state u8, reason u8 | per event |
   | `REST_RESULT` | route id u8, method u8, status u16 (GETs included, no body) | per call |
   | `TELNET_CMD` | text | per command |
   | `WEBHOOK` | state u8, HTTP status int16 | per call |
   | `FW_EVENT` | kind (OTA, FS, safeboot, PIC flash, upload, delete) u8, result u8, text | per event |
   | `HEALTH` | heap, largest block, minimum heap, worst stack high-water | every 60 s |
   | `SAT_EVENT` | event id u8, fixed fields | per decision (needs the SAT field list, see below) |
   | `MODE` | simulation/replay on/off | per event |

   Order of size at these rates: under 0.05 KB/s, so it fits the no-PSRAM budget next to the OT section.
3. **Debug text stays optional** (a per-subsystem selection), not the carrier of completeness.

## Decisions (Robert, 2026-11-15)

1. **Port 25238 bridge traffic: A.** Inbound bridge commands only, as text, only while the cloud recording runs.
   Needs a new ADR that supersedes the bridge exclusion of ADR-213; a command the ADR-217 SAT filter drops is
   recorded as refused. The outbound stream is not recorded (it is already in the OT section).
2. **REST GET: A.** Per request route id, method and status, no body. Consecutive identical GETs with status 200
   collapse into a per-minute counter; every other status is its own record.
3. **SAT: A.** While the cloud recording runs, SATTICK always goes to the SAT section, independent of
   `state.debug.bSAT` (`SATcontrol.ino:20`), and not to telnet. SAT behaviour does not change. Which SAT decisions
   become `SAT_EVENT` records follows from the firmware's own SAT code (TASK-1970): the recording is telemetry of the firmware, not of thermo-nova (Robert, 2026-11-15), so no Sergeant D question is needed.

## Not verified in this analysis

- Whether the streaming HA discovery path (ADR-077) goes through `recMqttPublish`.
- Whether PIC error lines (`Error 0x`, startup text) go through `recResponse`.
- Whether simulated frames are marked in the recording.

## Data per day (2026-11-15)

Basis: the field run of ADR-213 (`docs/diagnostics/recording-budget-1470.md`: PIC gateway, SAT on, 9,155 OT frames,
5,779 MQTT publishes, 1,047 PIC commands and 1,047 responses per hour) and the encoders on this branch. Measured
means measured on real or encoded data; estimate means a rate or size that was not measured.

| Part | KiB per day | Basis |
|---|---:|---|
| OT frames, range-coded (v2) | ~206 | 0.96 B/frame measured on the synthetic stream (`otrec.pack_ot2`); diagRec layout B needs 881 |
| MQTT publishes (incl. repeats) | 1,308 | measured, diagRec layout B |
| PIC commands + responses | 522 | measured, diagRec layout B |
| Weather, settings, MQTT in, reboot | < 10 | measured (< 10 per hour) |
| SATTICK always on (decision 3) | 110 to 670 | rate from ADR-213; 38 B/tick (estimate) up to 78 B/tick (this encoder, random values) |
| REST with GET collapsed (decision 2) | 30 to 60 | estimate; 230 without collapsing |
| Bridge inbound commands (decision 1) | < 10 | estimate |
| Dallas, S0, NET, MQTT_CONN, WEBHOOK, FW_EVENT, MODE, TELNET_CMD | 25 to 65 | estimate (3 sensors, on change + every 5 min) |
| HEALTH every 60 s | ~17 | estimate (12 B) |
| **Payload, full telemetry without debug text** | **~2.2 to 2.9 MB** | MQTT publishes are half of it |
| Debug text, no-PSRAM budget (~0.3 KB/s text) | + ~6.5 MB | measured, mix study |
| Debug text, everything on (PSRAM, token + zlib) | + ~30 MB | measured, debug study |

On the line and in storage (no debug text):

| Item | MB per day | Basis |
|---|---:|---|
| Payload as base64 in the JSON field (x 4/3), also what PocketBase stores | 3.0 to 3.9 | computed |
| HTTP headers, 1,440 records per day (one per 60 s) | ~1.3 | measured, batch study (~600 B up, ~280 B down per request) |
| TLS handshake per record (the PoC connects and stops per upload) | ~6 to 9 | estimate, 4 to 6 KB per full TLS 1.3 handshake with the certificate chain; not measured |
| **Total traffic** | **~10 to 14** | about 300 to 420 MB per month |
| Stored with 30 days retention | ~90 to 120 MB per gateway | |

Findings:

- **The TLS handshake costs more than the data.** The PoC (`CloudRecDevice.h:313-342`) makes a new TLS connection for
  every record. TLS session resumption (session tickets) or a connection that stays open brings that to well
  under 1 MB per day; resumption also saves the handshake heap peak. To measure on the bench (TASK-1963).
- **MQTT publishes are the largest part.** Most of them follow from OT frames. Range-coding the MQTT section like
  the OT section, or recording only the changed values (ADR-213: about -145 KiB per day), makes it smaller; leaving
  out derived publishes loses the build-against-build diff that ADR-213 is for. Not measured.
- **No-PSRAM buffer:** at about 34 B/s the 4 KB record fills in about 2 minutes, so the 60 s interval holds without
  debug text.
- The synthetic OT stream stands in for the field stream in the v2 figure; encoding the field stream with v2 is
  still to do.

## Storage per gateway (2026-11-15)

PocketBase stores the payload as base64 text (x 4/3); in the measured runs the database grew by about the base64
payload (mix study: 291 KB/h stored against 301 KB/h sent, headers included). Payload figures from the table above.

| Profile | Per day | 7 days | 30 days | 90 days | 1 year |
|---|---:|---:|---:|---:|---:|
| Full telemetry, no debug text | 3.0-3.9 MB | 21-27 MB | 90-117 MB | 270-350 MB | 1.1-1.4 GB |
| + debug text, no-PSRAM budget | ~12-13 MB | ~85-90 MB | ~360-390 MB | ~1.1 GB | ~4.5 GB |
| + all debug text (PSRAM, token + zlib) | ~43-44 MB | ~0.3 GB | ~1.3 GB | ~3.9 GB | ~16 GB |

- SQLite reuses the pages that the nightly retention delete frees, so the file levels off at about the retention
  window; it only shrinks with a `VACUUM` (`retention.pb.js` does not run one).
- PocketBase also logs every request in `auxiliary.db` (1,440 per gateway per day; estimate under 1 MB per day,
  kept for the log retention days set in the dashboard).
- A NAS backup (Hyper Backup) multiplies this by the number of versions it keeps.

## Decisions (Robert, 2026-11-15, second round)

1. SATTICK always to the recorder while it records (A), built in TASK-1974.
2. **Accepted: the Cloudflare edge can read the payload (C).** The tunnel ends TLS at Cloudflare; no device-side
   encryption is added. Anyone who sets up a collection point on this design should know this.
3. TLS session resumption (A), TASK-1974.
4. A second send slot on PSRAM boards only (A); without PSRAM a lost record stays reported in SEC_LOST.
5. The upload rate limit stays per IP (B).
6. Bench job: `docs/diagnostics/cloudrec-bench-job.md`.
7. PocketBase does not run on the Synology yet: the migration stays edited in place.

## Status after TASK-1473 (2026-11-15, 2.0.0-poc.7)

The cloud record now has an events section (SEC_EVT, `CloudRecCodec.h`); each capture point below sits inside
`#if HAS_CLOUD_REC` next to the ADR-213 capture point, or where the firmware had none. Decoded by `otrec.evt_decode`
into `events.log` (`decode_all.py`). Decision record: ADR-243 (Proposed).

| # | Source / event | Cloud recorder now |
|---|---|---|
| 1 | OT frames | ✓ OT section |
| 2-4 | PIC commands, OTDirect commands, PIC responses | ✓ `pic_cmd`, `otd_cmd`, `response` |
| 5 | Port 25238 bridge, inbound | ✓ `bridge_in` (refused flag for the ADR-217 filter); outbound stays out (it is the OT section) |
| 6-7 | MQTT in, MQTT value publishes | ✓ `mqtt_in` (redacted), `mqtt_pub` |
| 8 | MQTT connection | ✓ `mqtt_conn`; HA discovery publishes: not verified that they pass `sendMQTTData` |
| 9-10 | REST requests and results | ✓ `rest_in` (body, redacted), `rest` (every non-GET-200 with status), `rest_gets` (GET 200 counts per route per minute) |
| 11 | PIC flash, OTA, file upload and delete | ✓ `fw` |
| 12 | WebSocket | – (WS data is not used by the firmware; connects are in the debug text) |
| 13 | Telnet keys | ✓ `telnet_key` |
| 14 | Webhook | ✓ `webhook` (state, HTTP status) |
| 15 | Settings | ✓ `setting` (redacted) |
| 16-17 | Dallas, S0 | ✓ `dallas` (every reading, 1/128 degC exact), `s0` |
| 18 | PIC return/temp sensor | ✓ through `pic_cmd` / `response` |
| 19-20 | BLE, weather | ✓ `ble`, `weather` (float32, exact) |
| 21-22 | DHW water meter, GPIO outputs | derivable from the OT frames (not recorded separately) |
| 23 | SAT | ✓ SATTICK always; `SAT_EVENT` list still TASK-1970 |
| 24 | WiFi up/down | ✓ `net` (RSSI, IP or reason); Ethernet up/down: no event handler in the firmware, not recorded |
| 25-26 | NTP, reboot | ✓ `time_sync`, `reboot` (reset reason, once per boot) |
| 27-28 | Heap, health | ✓ `health` every minute (free heap, largest block, minimum, RSSI, uptime); losses in SEC_LOST |
| 29 | Simulation and telemetry mode | ✓ `mode` |

Still outside the recording: output that bypasses the `Debug*` macros (the telnet banner, file dumps).
