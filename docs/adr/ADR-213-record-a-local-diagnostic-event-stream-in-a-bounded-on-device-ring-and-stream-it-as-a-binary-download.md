---
id: "ADR-213"
title: "Record a local diagnostic event stream in a bounded on-device ring and stream it as a binary download"
status: "Accepted"
date: "2026-11-05"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "diagnostics"
  - "recording"
  - "replay"
  - "psram"
  - "rest api"
  - "streaming"
  - "privacy"
aliases:
  - "diagnostic recording"
  - "24 h recording"
  - "event recorder"
  - "flight recorder"
  - "/api/v2/diag/recording"
components:
  - "diagnostic recording ring"
  - "recording record format"
  - "GET /api/v2/diag/recording"
symbols:
  - "sendMQTTData"
  - "sendPICSerial"
  - "processOT"
  - "platformPsramAllocOnce"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-213 Record a local diagnostic event stream in a bounded on-device ring and stream it as a binary download

## Status

Accepted, 2026-11-05.

Guideline-level per ADR-080 (Binding ADR rules must have a CI (continuous integration) gate): no gate exists yet. The
implementation task adds a host test that encodes and decodes the record format and checks the
no-PSRAM allocation and refusal rule (see Verification). Robert answered the six open questions on
2026-11-05; the Decision below states those answers. The ADR stays Proposed until it passes the
four acceptance gates.

## Status History

```yaml
status_history:
  - date: 2026-11-05
    status: Proposed
    changed_by: "Agent: Claude"
    reason: Initial proposal (TASK-1470 AC1, from the AC2 measurements)
    changed_via: adr-kit
  - date: 2026-11-05
    status: Accepted
    changed_by: Robert van den Breemen
    reason: Accepted by Robert in the session 2026-11-05 after his six answers; the design TASK-1470 builds
    changed_via: adr-kit lifecycle
```

## Context

A tester proposed an opt-in 24 h diagnostic recording uploaded to a cloud store (TASK-1469).
The maintainer decided on 2026-10-29: local first, cloud later (TASK-1473). The recording must work
without any outbound traffic, because some units run in a VLAN (virtual LAN) with all internet blocked
(TASK-1469 notes). It must cover the same events as the proposal: OT (OpenTherm) frames, PIC (the gateway microcontroller) commands and
responses, MQTT (Message Queuing Telemetry Transport) publishes, external inputs (REST (Representational State Transfer) and MQTT commands, weather, BLE (Bluetooth Low Energy) readings, setting
changes) and reboots or crashes. A host replay plus a diff tool must then show the PIC-command and
MQTT-publish differences between two firmware builds (TASK-1470 AC3).

Measurements, 2026-11-05 (`docs/diagnostics/recording-budget-1470.md`):

- **Event rate.** A real boiler with a PIC gateway and SAT (Smart Autotune Thermostat) on (12.8 h field run) produces about
  17,000 events per hour: 9,155 OT frames, 5,779 MQTT publishes, 1,047 PIC commands and 1,047
  responses. The other types add fewer than 10 per hour. Cross-checked against the WebSocket frame
  log (0.01 % apart) and the broker (1.2 % apart).
- **Repeats.** 87 % of OT frames repeat the previous value for their source and MsgID; 63 to 84 %
  of MQTT publishes repeat the last value on their topic.
- **Size.** Encoding the real streams: 6.65 MB (megabytes) per day with a plain 6-byte header and topic text,
  4.85 MB with topic ids, **2.79 MB in the compact layout below** (32.2 B/s, 113 KiB/h, 4 KiB blocks that each decode
  on their own), about 2.05 MB without repeat records. A stream whose repeat cache is never reset
  would be 2.46 MB, but it cannot be decoded after the ring wraps.
- **RAM.** No-PSRAM unit (.61, beta.95, BLE off): 64 to 67 KB (kilobytes) free, largest block 31.7 KB,
  low-water mark 11,588 B after 58 min. The night run on the same unit reached a low-water mark of
  536 B (TASK-1528, open). BLE costs about 46 KB more (ADR-193). PSRAM (pseudo-static random-access memory) boards: 2,057,884 of
  2,097,152 B free after boot.
- **Flash.** App headroom 143,328 B (beta.99: 2,150,432 of 2,293,760 B). LittleFS: 768 KiB
  partition, the shipped image fills 161 of 192 blocks, 124 KiB free before runtime files.
- **Download.** The graph history streams about 28 KB in 0.25 to 0.36 s (about 100 KB/s, TASK-1306).

So 24 h of everything fits on no board. A no-PSRAM board has room for minutes at most.

Other constraints: the REST path runs on the async_tcp task and must not block or allocate a large
buffer (ADR-132, ADR-137); at most two API (application programming interface) requests in flight and at most four send-holding
connections (ADR-165, ADR-188, ADR-147); the PIC byte stream reaches every consumer verbatim
(ADR-177); large feature buffers are static or allocated once (ADR-053, ADR-193's PSRAM exception);
no ArduinoJson (ADR-146); admin routes follow ADR-056.

## Decision

1. **Opt-in, local, RAM (random-access memory) only.** Recording is off by default. The user starts and stops it in the
   web UI (user interface) or through REST. Records live in RAM and are lost on a reboot. A recording that was running
   resumes after a reboot as a new recording whose first record is the Reboot record (reset reason,
   crash summary of the previous boot); the "recording on" flag survives the reboot as a setting.
   On a no-PSRAM board the resume obeys the same start rule as a manual start (decision 3). The firmware never sends a recording
   anywhere; it only answers a download request from the LAN.

2. **Record format (version 1, compact).** A recording is a sequence of 4096-byte blocks.
   Every block decodes on its own: the repeat caches are cleared at each block start, so the first
   frame or publish of a key in a block is a full record. An overwritten block or a download that
   starts mid-stream never leaves a later block undecodable. A 4 KiB block costs 13 % more than a
   never-reset stream; 1 KiB blocks would cost 25 % more, 16 KiB blocks 6 % (field run).
   - Block header, 16 bytes: `block_seq u32` (counts up from the start of a recording),
     `uptime_ms u32` of the first record, `epoch u32` (0 before NTP (Network Time Protocol)), `first_record_seq u32`.
   - Record: `type u8`, `dt` (ms since the previous record, LEB128 (little-endian base-128 varint), 1 to 3 bytes), payload.
     Records never cross a block boundary.
   - A download starts with a file header: magic `OTGWREC`, format version, board
     (`HAS_*` flags, PSRAM present), firmware version and githash, boot count, reset reason,
     ring size, the redacted settings (decision 6) and the full topic and setting id tables.
     Topic ids come from a fixed table on the device, so the ring holds no dictionary records.

   | Type | Payload | Captured at |
   |---|---|---|
   | OT frame T, B, R, A, E (one type per source) | 4 bytes | `processOT()`, after the frame is decoded, for every source |
   | OT repeat | MsgID u8 (same frame as the last for that source and MsgID in this block) | same |
   | PIC command | len u8 + text | `sendPICSerial()`, which feeds `enqueuePICTx()`. The TCP (Transmission Control Protocol) serial bridge (port 25238) also writes through `enqueuePICTx()`, byte by byte; its bytes are not recorded as commands (decided: version 1 does not record port 25238 bridge bytes) |
   | OT-Direct command | len u8 + text | the OT-Direct command handler (the `handleOTDirectCommand()` path) |
   | PIC / OT-Direct response | len u8 + text | the response line handler |
   | MQTT publish | topic id u16, len u8, value (max 255, flag when cut) | `sendMQTTData()`, the value-publish path. HA (Home Assistant) discovery, the online/LWT message and discovery clears go straight to `mqttPublishRaw()` and are not recorded |
   | MQTT repeat | topic id u16 (same value as the last on that topic in this block) | same |
   | MQTT command in | len u8 + topic suffix, len u8 + value | the MQTT message callback |
   | REST command in | route id u8, method u8, len u8 + body (truncated) | REST dispatch, non-GET only |
   | Weather | outdoor temperature, humidity, wind, as int16 x100 | after a successful weather fetch |
   | BLE reading | sensor slot u8, temperature and humidity int16 x100, battery u8, RSSI (received signal strength) int8 | the loop-task side of the BLE reader (not the BLE callback) |
   | Setting change | setting id u16, len u8 + new value (secrets replaced, decision 6) | `updateSetting()` when a value changes |
   | Reboot | reset reason, boot count, crash summary of the previous boot | first record after boot |
   | Gap | count of records the writer had to drop | when an append fails (decision 4) |
   | Time sync | epoch u32 | when NTP sets or steps the clock |

   Repeat records keep the bus timing for replay at about a third of the cost.
   REST GETs are not recorded: they are reads (1,937 per hour with the web UI open).

3. **Buffer per board.**

   | Board | Ring | Storage | Holds at the field rate |
   |---|---|---|---|
   | With PSRAM | 1.5 MB | one PSRAM block allocated on the first start through `platformPsramAllocOnce()`, never freed (ADR-193 pattern); about 500 KB PSRAM stays free | about 13.5 h, about 18 h without repeats |
   | Without PSRAM | 4 KB (one block) | a heap block allocated when a recording starts and freed when it stops | about 2 min |

   On a no-PSRAM board the 4 KB ring is not a day-long recording: a host that pulls with `since=`
   at least every 2 minutes builds the continuous file (decision 5). The measured margin on the
   no-PSRAM unit (.61, BLE off) is a low-water mark of 11,588 B, so the ring leaves about 7.5 KB;
   with BLE on (about 46 KB more, ADR-193) there is no room. The recorder therefore refuses to
   start, and reports a reason in `/api/v2/diag/recording/status`, when the allocation would
   drop free heap below the ADR-030 5120 B rung, or when the allocation fails. The same check
   runs on a resume after a reboot. A refused start is not an error: the board is simply too tight.

   One writer lock guards the ring. The append copies at most 260 bytes and holds the lock for a
   few microseconds, so the serial, loop and async_tcp tasks can all write.

4. **Overflow.** The ring overwrites its oldest block. Gaps stay visible: a missing `block_seq`
   shows lost blocks (about 2 minutes each at the field rate), and a Gap record counts records dropped when the lock or the ring is not
   available. Nothing blocks or waits for room.

5. **Download.** `GET /api/v2/diag/recording?since=<block_seq>` answers `application/octet-stream`:
   the file header, then every block from `since` (or the oldest) to the newest. It streams with a
   filler callback like ADR-210, block by block, without a ring-sized copy. The block range is fixed
   when the request starts. A block overwritten during the download is left out, so it shows as a
   gap. `GET /api/v2/diag/recording/status` reports enabled, ring size, oldest and newest block
   sequence, dropped count and, when a start was refused, the reason, written with JsonEmit. The download holds one REST slot until it
   ends: about 15 s for 1.5 MB at the measured graph-history rate. The web UI makes one download
   call at a time and never adds it to a burst. A host script that pulls with `since=` every few
   minutes builds a continuous 24 h file on any board.

6. **Privacy.** The header and the setting records never carry passwords, tokens, MQTT
   credentials, the WiFi key or the admin password; they say "set" or "empty". Latitude and
   longitude are rounded to 0.1 degree. BLE MAC (hardware address) addresses become slot numbers. Recording and
   download are local only and need no account. The download and status routes are open (not
   admin-protected), like the crash log: anyone on the LAN (local area network) can fetch a recording. That is why this
   redaction (secrets, rounded location, BLE MAC slots) is mandatory and not best effort.

7. **Replay and diff (host side).** A host script decodes a recording. It writes the OT frames as
   `otframes.log`-style lines with their device time, so the existing replay
   (`POST /api/v2/simulate/start`, TASK-1294) plays them at the recorded pace (ADR-209). The
   recorded inputs (commands, weather, BLE, setting changes) are injected through their normal
   REST and MQTT paths at the recorded times. A diff script compares the PIC-command and
   MQTT-publish sequences of a recorded run with a replay on another build, aligned on time and
   MsgID. The scripts live in `scripts/`.

## Decision Contract

### Must

- Keep recording off until the user starts it, and record nothing outbound.
- Allocate the PSRAM ring once, through `platformPsramAllocOnce()`, on a board with PSRAM.
- On a board without PSRAM, allocate the 4 KB ring only when a recording starts and free it when
  the recording stops.
- Refuse to start (also on a resume after a reboot) and report a reason in the status when the
  allocation would drop free heap below the ADR-030 5120 B rung, or fails.
- Resume a running recording after a reboot as a new recording that starts with the Reboot record.
- Append without blocking: on contention or a full ring, drop and count (Gap record), never wait.
- Stream the download block by block with a filler callback; fix the block range when the request starts.
- Write a versioned file header so a decoder can refuse an unknown format.
- Replace every secret setting value before it enters the ring, round latitude and longitude to
  0.1 degree and replace BLE MAC addresses by slot numbers, because the download is open.

### Must Not

- Allocate a download buffer or copy the ring on the async_tcp task.
- Keep a ring allocated on a no-PSRAM board while no recording runs.
- Record the port 25238 bridge bytes in version 1.
- Write the recording to LittleFS, or send it to any host the user did not ask.
- Change the PIC byte stream or what other consumers see (ADR-177).

### Exceptions

- None.

### Verification

- Host test (implementation task): encode a fixture of events, decode them back, and check the
  block sequence, the gaps after an overwrite and the redaction of secret settings.
- Host test: the PSRAM ring allocation path calls `platformPsramAllocOnce()`; the no-PSRAM ring
  is allocated on start and freed on stop; a start is refused with a status reason when the free
  heap after the allocation would be below 5120 B (red on an unguarded start, green with the rule).
- Host test: a recording that was on before a reboot resumes with the Reboot record first.
- Bench: start a recording on a PSRAM unit, download it twice with `since=`, check the gap after
  an overwrite and that `hd_max_loop_gap_ms` and the REST 503 counters do not rise.
- TASK-1470 AC3: a recorded day replays on the host and the diff shows PIC-command and
  MQTT-publish differences between two builds.

## Alternatives Considered

- **Plain records with a 6-byte header (`seq u16, dt u16, type u8, len u8`).** Simpler to parse,
  but 4.85 to 6.65 MB per day against 2.79 MB. Rejected: it nearly halves the time a PSRAM ring holds.
- **One repeat cache for the whole stream and a topic dictionary in the ring.** 2.46 MB per day,
  but after a wrap the surviving repeats and topic ids refer to overwritten records. Rejected.
- **Text lines like `otframes.log`.** Readable without a decoder, but larger than the plain binary
  layout. Rejected for the ring. The host decoder writes this form for the replay.
- **Inputs and changed values only (no repeat records).** About 2.05 MB per day, so even a 1.5 MB
  ring holds only about 17 h. Rejected as the default: without repeats a replay loses the bus timing and the
  diff cannot see a repeated publish. Not chosen: the maintainer decided to keep repeat records (open question 4).
- **Inputs only, outputs regenerated by replaying on the old build.** Smallest, but the boiler (B)
  frames are open-loop in a replay, so the regenerated outputs are not the field outputs. Rejected.
- **A LittleFS ring.** Survives a reboot, but 124 KiB is free (about 66 minutes), every 4 KiB
  erase stalls the flash cache, and it wears the filesystem's free blocks (about 20 erases per
  block per day with perfect levelling). Rejected.
- **A small static ring on no-PSRAM boards (4 to 8 KB).** Holds 2.1 to 4.2 minutes at the field
  rate, and takes that much from a low-water mark of 11.6 KB measured with BLE off (536 B in the
  TASK-1528 night run). Rejected: a static ring is
  permanent, while the maintainer chose a 4 KB ring that exists only while a recording runs.
- **Open-source formats and compression.** nanopb 0.4.9.2 (zlib licence, active,
  github.com/nanopb/nanopb) and TinyCBOR v7.1 (MIT, active, github.com/intel/tinycbor) give a
  standard container, but a per-record tag and length costs as much as the whole compact record
  (4 to 7 bytes for an OT frame), and the host decoder is a few dozen lines either way. heatshrink
  (ISC (ISC licence), github.com/atomicobject/heatshrink, last push 2024-05) could compress blocks further, but
  it adds a dependency and work on the writer path for data that is already delta-coded. Not
  adopted; the format version leaves room for a compressed block type later.
- **Status quo: host capture only (`scripts/otgw_capture.py`).** Works on every board but needs a
  PC (personal computer) for 24 h, and does not see PIC commands, setting changes or internal timing in a structured
  form. Kept as the fallback.
- **Cloud upload (the tester's proposal).** Parked by the maintainer (TASK-1473).

## Consequences

**Positive:**

- A PSRAM board keeps about 13.5 h of everything without a PC; a host that pulls with `since=` makes
  a full day on any board.
- One versioned format serves the download, the replay and the diff.
- No outbound traffic and no new dependency.

**Negative:**

- A 1.5 MB PSRAM block stays allocated after the first start (until a reboot); about 500 KB of
  PSRAM stays free.
- Hooks in five hot paths (`processOT`, `sendPICSerial`, `sendMQTTData`, the response handler,
  REST dispatch) add one call each. The cost per append is not measured.
- HA discovery, the online message and the serial bridge traffic are not in the recording.
- A download holds a REST slot for about 15 s and the web UI waits behind it.
- Anyone on the LAN can fetch a recording, because the download is open. Mitigation: the mandatory
  redaction of secrets, location and BLE MAC addresses.
- A no-PSRAM board holds only about 2 minutes (one 4 KB block), so a full record needs a host that
  pulls every 2 minutes or less. The ring costs 4 KB of a 7.5 KB margin on the measured unit
  (low-water mark 11,588 B, TASK-1528 open), and with BLE on there is no room. Mitigation: the
  recorder refuses to start when the free heap after the allocation would be below the ADR-030
  5120 B rung, and reports the reason in the status.
- A recording that was on before a crash resumes on its own after the reboot, so a no-PSRAM unit
  that is tight on heap can refuse the resume; the status shows why.
- About 6 to 12 KB of code (estimate) from 140 KiB of app headroom.

## Open Questions

- [x] No-PSRAM boards: (a) no on-device ring, host pull only; (b) a 4 KB ring allocated only while recording runs; (c) an 8 KB static ring. Recommendation: (a) until TASK-1528 is fixed. - **Answered 2026-11-05 by Robert van den Breemen:** A 4 KB ring, allocated only while a recording runs and freed when it stops (holds about 2 min). Robert 2026-11-05
- [x] PSRAM ring size: 512 KB (about 4.5 h), 1 MB (about 9 h) or 1.5 MB (about 13.5 h, about 500 KB PSRAM left). Recommendation: 1 MB. - **Answered 2026-11-05 by Robert van den Breemen:** 1.5 MB (about 13.5 h, about 500 KB PSRAM left). Robert 2026-11-05
- [x] Download access: admin-protected like ADR-210, or open like the crash log. Recommendation: admin-protected, because the recording holds commands and settings. - **Answered 2026-11-05 by Robert van den Breemen:** Open, not admin-protected. Robert 2026-11-05
- [x] Content: keep repeat records (default, 2.79 MB per day), drop MQTT repeats (about 2.64 MB) or drop all repeats (about 2.05 MB; no choice makes a full day fit). Recommendation: keep repeats. - **Answered 2026-11-05 by Robert van den Breemen:** Keep repeat records (about 2.8 MB per day). Robert 2026-11-05
- [x] Recording state across a reboot: always stopped after a reboot, or resumed automatically. The RAM ring is lost either way; resume starts a new recording whose first record is the reboot and crash summary. Recommendation: resume, so a crash during a recorded period stays visible. - **Answered 2026-11-05 by Robert van den Breemen:** A running recording resumes after a reboot as a new recording whose first record is the reboot and crash record. Robert 2026-11-05
- [x] Serial bridge (port 25238, OTmonitor): leave its bytes out, or record them as line records. Recommendation: leave out in version 1; the bridge is a raw byte path (ADR-177). - **Answered 2026-11-05 by Robert van den Breemen:** Port 25238 bridge bytes are not recorded in version 1. Robert 2026-11-05

## Related Decisions

- **ADR-193 (graph history ring):** the PSRAM allocate-once pattern and the chunked download.
- **ADR-210 (core dump download):** the binary filler download (its admin protection is not applied here; see decision 6).
- **ADR-209 (timed replay):** the replay pace for decoded frames.
- **ADR-165, ADR-188, ADR-147:** the REST slot, connection and file-serve gates the download runs under.
- **ADR-123 (single MQTT publish chokepoint `mqttPublishRaw()`):** the recorder hooks the value path above it.
- **ADR-177 (PIC byte stream verbatim):** the recorder is one more consumer and changes nothing upstream.
- **ADR-053 (static buffers), ADR-030 (heap ladder: the 5120 B start rule), ADR-056 (admin routes, not applied to the download), ADR-146 (no ArduinoJson).**

## References

- Measurements: `docs/diagnostics/recording-budget-1470.md`.
- Tasks: TASK-1470, TASK-1469 (proposal), TASK-1473 (cloud, parked), TASK-1294 (replay),
  TASK-1306 (graph history download timing), TASK-1528 (heap low-water mark).
- Source: `src/OTGW-firmware/MQTTstuff.ino` (`sendMQTTData`), `src/OTGW-firmware/OTGW-Core.ino`
  (`processOT`, `sendPICSerial`, replay), `src/OTGW-firmware/OTDirect.ino`, `src/OTGW-firmware/restAPI.ino`.
