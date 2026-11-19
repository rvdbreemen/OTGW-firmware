---
id: TASK-1473
title: >-
  Opt-in cloud telemetry: the full event stream (inputs, outputs, sensors) in
  the cloud recorder, bridge commands included
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T12:45:00Z'
updated_date: '2026-11-15T23:05:00Z'
labels:
  - diagnostics
  - tooling
  - cloud-recording
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 633000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Released by Robert 2026-11-15 ('Bouw TASK-1473'). The original plan (Cloudflare Worker + R2, PSRAM boards only) was replaced by the research PoC of TASK-1862/1938/1963/1971/1972/1974/1979: PocketBase self-hosted, outbound TLS with dynamic mbedTLS buffers also without PSRAM, one LZ-coded record a minute. This task adds the rest of the telemetry decided in docs/diagnostics/cloud-telemetry-completeness.md: an events section with everything the firmware receives, sends or measures besides OT/SATTICK/debug, the port 25238 bridge commands inbound (decision 1 A), REST results with GET 200 counted per route per minute (decision 2 A), and an ADR for the whole design. Research branch only (esp32-combo-tlsdyn, HAS_CLOUD_REC).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Events section (SEC_EVT) in CloudRecCodec.h with a host decoder; every event type round-trips through the real recorder
- [x] #2 Capture points for the diagRec events, sensors outside the bus, WiFi, MQTT connection, REST results, telnet keys, webhook, firmware events, health, mode and the bridge, each inside #if HAS_CLOUD_REC
- [x] #3 No new test failures against the HEAD baseline; esp32-combo-tlsdyn builds; evaluate --quick no new FAIL
- [x] #4 ADR for the cloud telemetry written (Proposed) and indexed
- [ ] #5 Events visible on a device in events.log (bench check 15)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Phase 0 - gates before any code (when Robert releases the task):
- Robert removes the on-hold/parked labels.
- Bench measurement on a LOLIN S3 Mini (PSRAM): heap peak and steady state of one persistent TLS connection to *.workers.dev with the Arduino core NetworkClientSecure (mbedTLS) + CA bundle; check whether mbedTLS can allocate in PSRAM with the prebuilt Arduino libs. Go/no-go for the ADR.
- Verify on a free Cloudflare account: R2 bucket with jurisdiction eu can be created on the free plan; whether a payment method is required; set a lifecycle rule (retention, e.g. 30 days).
- Open-source check per CLAUDE.md (>=2 candidates) for any helper (e.g. compression, ring buffer).

Phase 1 - ADR (adr-kit, Proposed -> Accepted by Robert):
- Outbound TLS client only, PSRAM boards only, opt-in; server side stays HTTP (partial supersede of ADR-003; ADR-003 text 'reject any PR adding TLS' needs the explicit carve-out).
- Recording format shared with TASK-1470: OT record = epoch_us (or delta), source, frame type, 32-bit frame; debug record = epoch_us + text line (<=255 B). Chunk header: device id (random, not MAC), firmware version, board, sequence number, gap counter.

Phase 2 - firmware (shared with TASK-1470 where possible):
- Timestamp at reception: add a us timestamp to OTFrameMsg (OTGW-Core.h:577) set in enqueueOTFrame (PIC task, OTDirect bridge, replay), instead of millis() at dequeue (OTGW-Core.ino:5371).
- Debug tee: wrap the three debugTelnet.print points (debugStuff.h:69-70, debugStuff.ino:40, :113) into a recorder sink; note the banner and file dump bypass the macros.
- Recorder: two ring buffers in PSRAM (platformPsramAllocOnce), drop-oldest with a gap counter.
- Uploader: own FreeRTOS task like the webhook task (webhook.ino, ADR-136/137): every 10-30 s one HTTPS POST per stream over a persistent connection, bearer token per device, exponential backoff, one log line per retry interval; HAS_* flag + psramFound() gate.
- Settings (ADR-051): enable, endpoint URL, token, streams on/off, debug module filter. Web UI toggle + consent text.

Phase 3 - Worker (repo dir e.g. tools/cloud-recorder/, ~100-200 lines):
- POST /ingest/{stream}: check token, write chunk to R2 (key device/stream/date/seq), no processing of content; rate limit per token.
- Download for the developers (authenticated) -> host tool converts OT chunks to a timed replay file.

Phase 4 - replay:
- Replay with timestamp pacing instead of the fixed 750 ms (debugStuff.h:59, OTGW-Core.ino:4278); host tool that turns an R2 OT stream into that format.

Phase 5 - verification: 24 h bench run with upload on, then with outbound blocked (TASK-1471 method); measure real volume per stream; replay a recorded day and compare with the original timing.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 Robert (interactive, in session), research + decisions:
- Wants a capture-like remote recording: extensive debug logging (firmware, boiler, thermostat, SAT) plus a SEPARATE OT stream with the reception timing stored, for real replays. Minimal maintenance, free, data stored in the EU, no separate Python script.
- Decided: CONTINUOUS streaming from the device itself (not browser relay, not local-only). Backend: Cloudflare Worker + R2 bucket with EU jurisdiction. Transport: outbound TLS client, PSRAM boards only (precedent ADR-169); needs a new ADR that partly supersedes ADR-003 (web server stays HTTP, only the outbound upload uses HTTPS).
- Rejected: Google (GCS free tier US-only, billing account required), MQTT cloud brokers (TLS+SNI mandatory, no region choice on free HiveMQ, transport not storage), Hetzner VPS (maintenance), WireGuard-ESP32 (stale since 2024, core 3.x issues), payload encryption over plain HTTP.
- Considered: Grafana Cloud Free EU (Loki, 50 GB/month, 14 d) and Hetzner Storage Box BX11 (~EUR 3.20/month).
Firmware facts found (code survey, spot-checked):
- OTFrameMsg has no timestamp (OTGW-Core.h:577); time is taken at dequeue (OTGW-Core.ino:5371). Needs a us timestamp at enqueueOTFrame (PIC task tick 2 ms; OTDirect better).
- Replay plays at a fixed 750 ms per line (debugStuff.h:59), no timestamp pacing.
- Debug text goes to telnet only (1 client, 512 B TX ring, 256 B line buffer); hook points debugStuff.h:69-70, debugStuff.ino:40 and :113.
- No WebSocket client lib, no TLS client in firmware code; webhook task (webhook.ino) is the pattern for an uploader task (ADR-136/137); must fail quietly per TASK-1471.
- OTGW32 has no PSRAM (minFree ~24.7 KB), so TLS handshake (~40-50 KB peak, estimate) only on PSRAM boards.
Open / unverified: EU jurisdiction on the R2/DO free plan; whether R2 asks for a payment method; mbedTLS allocation in PSRAM with the prebuilt Arduino libs; real data volume (estimates: OT ~9 MB/day, full debug up to ~260 MB/day). Task stays parked until Robert removes the parked label (AC #1).

Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: parked on purpose.

Robert 2026-11-03 (interactive): keep ON HOLD; not the moment yet.

2026-11-04 Robert (interactive): moved to milestone 2.1.0, out of the 2.0.0 scope.

Robert 2026-11-05 (session otgw-firmware-54, AskUserQuestion): stays parked until 2.1.0; labels parked/on-hold stay.

2026-11-14 encoding study for this feature: docs/diagnostics/ot-stream-encoding-study.md (see TASK-1862 notes). Task stays on hold.

2026-11-14 #dev-sat-mqtt (see TASK-1862, owned by Robert's cloud session): Robert to Sergeant D: 'I'm really looking into building an opt in feature to get data from users so you have more to work with.' https://discord.com/channels/812969634638725140/1105556725714649128/1571146379177234436 ; Sergeant D: 'Even with data only from gruijter we made the software even more robust.' https://discord.com/channels/812969634638725140/1105556725714649128/1571146720300105811 (after 'We don't need to be in a hurry', 1571146149442756669). Local per-user field-data archive exists since 2026-11-14 (RvdB/OTGW-field-data, Robert 1571077884976701491).

2026-11-15 built (2.0.0-poc.7). SEC_EVT = 8, 1,408 B (OT 384, SAT 512, debug 1,792; record still 4,096 + 32): 23 event types (EV_* in CloudRecCodec.h; layouts in otrec.EV_LAYOUT; texts LEB length + bytes, floats exact float32, Dallas in exact 1/128 degC steps, cut texts counted as lost). tests/test_cloudrec_codec.py new test: 900 events of every type through the real crec::Recorder, records close when the section is full, every event back in order with its ms time; 5/5 OK. Capture points (32, each in #if HAS_CLOUD_REC so test harnesses that cut real functions stay unchanged): next to the ten diagRec points (OTD commands from the bridge excluded, they go as bridge_in), REST body + result after the handler (restResponseStatus, single async_tcp task), 429 path, Dallas every reading, S0, WiFi connect/disconnect, MQTT connect/disconnect, telnet key, sensor/OTGW simulation switches, webhook result, PIC flash start/done, OTA start/end, FSexplorer upload/delete, bridge lines on both paths (refused flag on the PIC path), health + GET counts every minute, reboot once per boot, central mode change. Not recorded: Ethernet up/down (no event handler exists), WebSocket data (unused). Test suite: 46 failing files on a clean HEAD worktree and the same 46 with the change (pre-existing in this container), no new failure. Build esp32-combo-tlsdyn exit 0: RAM 156,772 B, app 2,258,351 B (98.5 %), 18 cloudRecEv functions linked, no new warnings in the touched files. evaluate --quick 0 FAIL 0 WARN. ADR-290 (Proposed, guideline-level) + README index; tests/test_adr_governance.py OK. Docs: completeness doc status table, tools README, bench check 15. AC 5 needs the bench.

2026-11-15 Robert: 'Akkoord met ADR 243' -> ADR-290 Accepted (status, history, README index). Device evidence still to come from the bench job.
<!-- SECTION:NOTES:END -->
