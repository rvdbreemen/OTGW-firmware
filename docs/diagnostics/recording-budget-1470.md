# Local diagnostic recording: measured budget (TASK-1470 AC2)

Date: 2026-11-05. Base: `origin/dev` 9d171aa2c (firmware 2.0.0-beta.95 to beta.99 on the units).
Decision record: [ADR-213](../adr/ADR-213-record-a-local-diagnostic-event-stream-in-a-bounded-on-device-ring-and-stream-it-as-a-binary-download.md).

## Summary

- A real boiler with a PIC gateway and SAT on produces about **17,000 recordable events per hour**:
  9,150 OT frames, 5,780 MQTT publishes, 1,050 PIC commands and 1,050 PIC responses.
  Everything else (weather, inbound commands, setting saves) adds fewer than 10 per hour.
- Encoded with the real event streams, that is **2.79 MB per day in a compact layout whose 4 KiB
  blocks each decode on their own** (32.2 B/s, 113 KiB/h). A plain layout with a 6-byte header per
  record needs 4.85 to 6.65 MB per day. The tester's estimate (TASK-1469: 150 to 200 B/s, 13 to 17 MB
  per day) was 4 to 6 times too high.
- **24 h of everything fits on no board.** A 1 MB PSRAM (pseudo-static RAM) ring holds about 9 h.
  Leaving out the repeat records (a frame or publish with the same value as earlier in the block)
  brings a day to about 2.05 MB, which still does not fit next to the rest of PSRAM. Without PSRAM,
  the free heap leaves room for a ring of a few KB at most, which is minutes.
- LittleFS cannot hold a day either: the shipped image leaves 124 KiB free in the 768 KiB partition.
- The firmware image has 140 KiB of headroom (beta.99: 2,150,432 of 2,293,760 B), not the
  ~196 KB the task description still quotes.

## Sources and conditions

| Run | Board | What it is | Length | Used for |
|---|---|---|---|---|
| Field run, 2026-11-04 (tester, private) | Classic PIC gateway, ESP32-S3 with 2 MB PSRAM | Real thermostat and boiler, SAT on, BLE on (no sensor accepted), web UI open in a headless browser, beta.68 | 12.8 h daytime (08:37 to 21:23) | Event rates. Only aggregates are used; no capture content is in this repo. |
| Night run, TASK-1528, 2026-11-04/05 | OT-Direct bench unit .61, no PSRAM | OT replay (`events.log`: "replay start ok"), router drop injected every 30 min, beta.79 | 6.8 h, 5.85 h with telnet connected | Rates on a synthetic bus; heap low-water mark |
| PIC bench capture, 2026-11-05 18:00 | Classic PIC bench unit .63 | 2 min, PIC only sees `R 80000200` (no thermostat or boiler on the bus), beta.95 | 2 min | Shows a bench without a bus is not representative; not used for rates |
| Read-only probe, 2026-11-05 18:17 to 18:18 | .61, no PSRAM, beta.95+1fbd5cf, uptime 58 min | 6 × `GET /api/v2/device/info`, one per 10 s. BLE off (`satbleenable` false in the saved settings). Web UI state unknown: device/info has no WebSocket client count, and a bench agent may have been using the unit. | 1 min | RAM headroom now |

Scripts (host-side, kept outside the repo in the session scratchpad `r1470/`): `analyze.py` (rates), `encsim.py` and `encsim2.py` (encode the real
event streams in the candidate layouts and count the bytes), `heap.py` (heap statistics from the
`(free|maxblock)` column of every telnet line). They read the telnet debug log and `otframes.log`.

**Cross-checks.** OT frames: 116,889 in `otframes.log` (WebSocket) against 116,878 decoded
`processOT` lines in telnet (0.01 % apart). MQTT publishes: 73,805 `sendMQTTData` lines in
telnet against 74,667 messages on the device's `/value/` topics at the broker (1.2 % apart; the recorder hooks `sendMQTTData()`, so HA discovery and the online message, which go straight to `mqttPublishRaw()`, are outside that count; the
telnet log can drop lines under an MQTT burst). The field run is the only real-boiler source,
and it covers daytime only; the 24 h numbers extrapolate it.

## Event rates

Per hour, from the field run (real boiler) and the night run (bench replay with injected drops):

| Event type | Field (PIC, real boiler) | Night (OT-Direct, replay) | Notes |
|---|---:|---:|---|
| OT frames T/B/R/A | 9,155 (T 3,593, B 3,593, R 984, A 985) | 4,783 | Night = replayed frames. E (error) frames: 0 |
| MQTT publishes (`sendMQTTData`) | 5,779 | 5,439 | 258 and 249 distinct topics |
| PIC commands (`sendPICSerial`) / OT-Direct commands | 1,047 | 790 | mean 6.3 to 6.7 characters |
| PIC / OT-Direct responses (`XX: ...` lines) | 1,047 | 790 | mean 8.5 characters |
| SAT control ticks (`SATTICK`) | 120 | 360 | SAT state, optional in a recording |
| BLE scan windows | 120 | 0 | 0 readings accepted: no sensor configured. A real sensor's rate is **not measured** |
| Weather fetches | 3.8 | 3.4 | one per ~15 min |
| Inbound MQTT commands | 0.16 (2 arrived on the device) | 0 | The broker saw 38 `/set/` messages for the device in the same run. The two counts disagree; not resolved |
| Setting saves (`writeSetting`/`flushSetting`) | 0.6 / 0.16 | ~0 | `updateSetting` lines (32/h) are per key, not per change |
| REST writes (non-GET) | 0 | 0 | |
| REST GETs | 1,937 | 353 | Web UI polling. Reads, not commands: not budgeted as recorded events |
| Reboots | 0 | 0 | one header record per boot |

Two properties of the streams decide the compact layout:

- **87 %** of OT frames carry the same value as the previous frame with the same source and MsgID (field run).
- **63 %** (field) and **84 %** (night) of MQTT publishes repeat the last value on that topic.
- OT frame gaps: 61 % below 256 ms, the rest below 2.56 s, one above 65 s.
- MQTT topic after the `<prefix>/value/<node>/` part: mean 14 to 16 characters; value mean 6.2 characters;
  787 publishes in 12.8 h (61/h) have a value longer than 64 characters (up to 402).

## Record layouts and bytes per day

Every layout starts a file with a header record (format version, board, firmware version and
githash, boot count and reset reason, redacted settings). The candidate layouts per event:

**Layout A (plain).** 6-byte header per record: `seq u16, dt u16 (ms since the previous record,
saturating), type u8, len u8`, then the payload. OT frame = 4 bytes. MQTT: A1 = topic suffix +
NUL + value; A2 = topic id u16 + value, with a dictionary record the first time a topic is seen.

**Layout B (compact).** Records live in 4096-byte blocks. Each block has a 16-byte header:
`block seq u32, uptime ms u32, epoch u32, seq of the first record u32`. A gap is a jump in the
block sequence. A record is `type u8, dt (LEB128 ms), payload`. **Every block decodes on its own:**
the repeat caches (last frame per source and MsgID, last value per topic) are cleared at each block
start, so the first frame or publish of a key in a block is always a full record. Topic ids come from
a fixed table on the device, written in full in the download header, not as ring records; an
overwritten block can therefore never orphan a later one.

| Type | Payload |
|---|---|
| OT frame (one type per source T/B/R/A/E) | 4 bytes |
| OT repeat (same frame as the last for that source and MsgID in this block) | MsgID u8 |
| MQTT publish | topic id u16, len u8, value |
| MQTT repeat (same value as the last on that topic in this block) | topic id u16 |
| PIC / OT-Direct command, PIC response, REST or MQTT command | len u8, text |
| Weather, BLE reading, setting change, reboot/crash | small fixed structs (see ADR-213) |

Bytes per day, computed by encoding the actual event streams (`encsim.py` for layout A and the
never-reset lower bound, `encsim2.py` for self-decoding blocks of 1, 4 and 16 KiB), extrapolated
from the active hours to 24 h. Field run:

| Event type | A1 plain, topic text | A2 plain, topic id | B, 4 KiB self-decoding blocks | B 4 KiB, repeats dropped |
|---|---:|---:|---:|---:|
| OT frames | 2,145 KiB | 2,145 KiB | 881 KiB (new 307 + repeat 574) | 307 KiB |
| MQTT publishes | 3,690 KiB | 1,935 KiB (incl. dictionary 13) | 1,308 KiB (new 1,163 + repeat 145) | 1,163 KiB |
| PIC commands | 301 KiB | 301 KiB | 239 KiB | 239 KiB |
| PIC responses | 357 KiB | 357 KiB | 283 KiB | 283 KiB |
| Block headers | - | - | 11 KiB | 11 KiB |
| Small types (weather, inbound commands, settings, reboot; estimate) | < 10 KiB | < 10 KiB | < 10 KiB | < 10 KiB |
| **Total per day (field)** | **6.65 MB** | **4.85 MB** | **2.79 MB** | **~2.05 MB** |
| Rate | 76.9 B/s | 56.1 B/s | 32.2 B/s | ~23.7 B/s |
| Per hour | 270 KiB | 197 KiB | 113 KiB | ~83 KiB |

Block size against cost (field run, self-decoding): 1 KiB blocks 3.08 MB per day, 4 KiB 2.79 MB,
16 KiB 2.61 MB. A cache that is never reset gives 2.46 MB, but such a stream cannot be decoded once
the ring has wrapped or when a download starts mid-stream, so it is only a lower bound.
4 KiB is the chosen trade-off: one overwrite loses about 2 minutes, and the cost is 13 % above
the lower bound. Night run (bench replay, OT-Direct): A1 5.44 MB, A2 3.55 MB, B 4 KiB 2.09 MB per day.

The "repeats dropped" column keeps only the first frame or publish per key in a block and changes
after it. A replay then cannot reproduce the bus timing, and a diff cannot show a publish that one
build repeats and the other does not. Dropping only MQTT repeats: about 2.64 MB per day.

Optional extras, not in the totals: SAT control ticks at about 38 bytes each = 109 to 328 KiB per day;
REST GETs as route id + status = about 230 KiB per day with the web UI open; BLE readings (rate not measured).

## RAM headroom

| Measurement | Free heap | Largest block | Low-water mark | Source |
|---|---:|---:|---:|---|
| .61, no PSRAM, BLE off, 58 min uptime | 63,924 to 67,444 B (internal 55,632 to 58,752) | 31,732 B | 11,588 B (`hd_min_free_heap`), largest block low 19,444 B | read-only probe above |
| .61 night run, no PSRAM, replay + drops | median 50,880 B, 1st percentile 42,700 B | median 24,564 B, low 1,524 B | **536 B** (21:33:06), 548 B at 21:21:49 | `samples.log` + telnet column; the open bug TASK-1528 |
| Field, PSRAM board, BLE on, web UI open | median 54,720 B, 1st percentile 50,036 B, low 39,244 B (internal heap) | 31,732 B, low 26,612 B | 19,056 B | telnet column + device/info |
| BLE cost on a no-PSRAM unit | about -46 KB | max block 38.9 to 21.5 KB | | ADR-193 (bench 2026-10-22) |
| PSRAM boards | psram_free 2,057,884 of 2,097,152 B after boot (.63, beta.95, graph ring included); field 2,054,728 B | | | TASK-1306 notes, field device/info |

Heap figures 63,360 / 61,808 for the TASK-1306 AC1 run were in the brief but are not in the task
notes; they are not used here.

**What a ring of N KB leaves on a no-PSRAM board.** A static ring comes straight out of the heap,
so every figure above drops by N. ADR-030's ladder (3,072 / 5,120 / 8,192 B) is the floor for
emergency recovery.

| Ring | Low-water mark left (probe, BLE off: 11,588 B) | Night run (536 B) | With BLE on (about -46 KB) | Holds at field rate (layout B, 113 KiB/h) |
|---|---:|---:|---:|---:|
| 4 KB | ~7.5 KB | below zero | no room | ~2.1 min |
| 8 KB | ~3.4 KB (below the 5,120 B rung) | below zero | no room | ~4.2 min |
| 16 KB | below zero | below zero | no room | ~8.5 min |

Conclusion: on a no-PSRAM board any on-device ring larger than 4 KB eats the margin that keeps the
unit out of the CRITICAL tier, and the night run shows the margin can already be gone (TASK-1528
is open). A ring of a few minutes is the most such a board can offer, and only with BLE off.

**PSRAM boards.** 2.05 MB free after boot, the graph ring (ADR-193) included. A 1 MB ring holds
about 9 h of layout B (field rate, 113 KiB/h) or about 12 h with repeats dropped. A 1.5 MB ring holds
about 13.5 h, and leaves about 500 KB of PSRAM for the rest
(BLE stack, future buffers). Not measured: how PSRAM bandwidth and the ring writes from the serial
task and the loop task interact with the WebSocket and REST paths.

## Flash cost

- **App image.** app0 is 2,293,760 B. beta.95 is 2,149,280 B (measured, `build/*.ino.bin`), beta.99
  is 2,150,432 B (from the brief). Headroom about 143,328 B (140 KiB). A recorder (ring, encoders,
  hooks, download route) is estimated at 6 to 12 KB of code. **Estimate, not measured.**
- **LittleFS.** The partition is 768 KiB (192 blocks of 4 KiB). The beta.95 image fills 161 blocks
  (659,456 B, measured on `build/*.littlefs.bin`), leaving 31 blocks (126,976 B) before any
  runtime file (`settings.ini` changes, SAT state, logs). That is about 66 minutes of layout B at
  the field rate.
- **Wear of a LittleFS ring.** At 2.79 MB per day spread over about 31 free blocks, each block is
  erased about 22 times per day with perfect wear levelling. LittleFS commits metadata on top
  of that, so the real count is higher unless records are buffered into whole 4 KiB writes.
  The endurance of the fitted flash parts (LOLIN S3 Mini, OTGW32 module) is **not verified**: look
  it up in the module datasheet before choosing a flash ring. A 4 KiB erase also stalls the flash
  cache, which pauses both cores for tens of milliseconds (not measured on this firmware).
- **RAM ring against a LittleFS ring.** LittleFS adds persistence across a reboot, not duration
  (124 KiB < one PSRAM MB), and it costs wear and erase stalls. A RAM ring loses its content on a
  reboot. A small crash-surviving tail (RTC or no-init RAM) would cover that case. Whether the
  pinned Arduino core keeps a no-init PSRAM segment across a reset is not verified.

## Download

- The graph history (ADR-193, TASK-1306) streams 28.2 to 28.8 KB in 0.25 to 0.36 s on both boards,
  about 100 KB/s. That is a chunked JSON response; a binary filler (ADR-210) should not be slower,
  but that is **not measured**.
- At that rate a 1 MB ring takes about 10 s, 2.5 MB about 25 s. The download holds one of the two
  REST in-flight slots (ADR-165) for that time, and the v2 bundle's fetch queue (two in flight)
  makes the web UI wait behind it.
- ADR-188 counts the download connection as one of the four send-holding connections while it runs.
- The web UI starts one download at a time and never runs it beside a burst of other API calls.

## Not measured

- 24 h of a real boiler: the field run is 12.8 h of daytime. Night behaviour (lower heat demand,
  fewer SAT changes) is extrapolated.
- An OT-Direct board on a real boiler: the OT-Direct numbers come from a replay.
- BLE reading rate with a real sensor accepted.
- REST and MQTT command rates in normal use (both near zero in the field run; the device and the
  broker disagree on inbound MQTT commands).
- RAM headroom on the no-PSRAM board with the web UI open and BLE on at the same time; the probe
  could not tell whether the web UI was open.
- Ring write cost on the serial task and the loop task, PSRAM contention, binary download throughput.
- Flash endurance of the fitted parts and the stall of a LittleFS erase on this firmware.
- Code size of the recorder.
