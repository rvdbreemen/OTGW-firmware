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

## Decisions for Robert (one at a time, by DM)

1. **Port 25238 bridge traffic**: ADR-213 leaves it out on purpose. Recording it needs a new ADR that supersedes
   that part. Recommendation: commands in only, as text, and only while the cloud recording runs.
2. **REST GET**: status code and route only, no body (recommended), or not at all.
3. **SAT_EVENT fields**: which SAT decisions belong in a record is a SAT behaviour question and goes to Sergeant D
   under `.claude/rules/sat-ground-truth.md`; until then SATTICK (debug on) is the SAT source.

## Not verified in this analysis

- Whether the streaming HA discovery path (ADR-077) goes through `recMqttPublish`.
- Whether PIC error lines (`Error 0x`, startup text) go through `recResponse`.
- Whether simulated frames are marked in the recording.
