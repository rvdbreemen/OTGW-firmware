---
id: TASK-1473
title: >-
  On hold: continuous opt-in cloud recording (debug + timed OT stream) to
  Cloudflare R2 EU over outbound TLS, PSRAM boards
status: To Do
assignee: []
created_date: '2026-10-29T12:45:00Z'
updated_date: '2026-10-30T06:13:00Z'
labels:
  - parked
  - on-hold
  - needs-maintainer-ok
  - diagnostics
  - tooling
dependencies: []
priority: low
ordinal: 633000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ON HOLD (Robert 2026-10-30): worked out, NOT to be implemented until Robert says it is the right moment. Low priority. Do not pick this up in /implement-next-task.

Goal: a capture-like recording without a separate script, so Robert can check how the firmware, the boiler, the thermostat and SAT behave after the fact. Two separate streams:
1. Debug stream: a copy of all telnet debug output (all modules), with timestamps.
2. OT stream: every OpenTherm frame (T/B/R/A, source PIC/OTDirect/replay) with its reception time in microseconds (epoch), compact, so a day can be replayed with the real timing.

Decisions (Robert, interactive, 2026-10-30):
- Continuous streaming from the device itself (not a browser relay, not local-only).
- Backend: Cloudflare Worker + R2 bucket with EU jurisdiction (free tier, serverless, minimal maintenance).
- Transport: outbound TLS client (HTTPS POST batches), on PSRAM boards only (precedent ADR-169). Web server stays HTTP. Needs a new ADR that partly supersedes ADR-003.
- Opt-in, off by default; local-first stays the default (TASK-1470 shares the recording format).
Hard constraints: fail quietly on a LAN with internet blocked (TASK-1471): drop/buffer, no loop stall, no reboot, no log flood. No ArduinoJson (ADR-146). No String in hot paths (ADR-180). No blocking network call in the loop or on async_tcp (ADR-137). Raw PIC stream rules (ADR-177) apply to any new consumer. Secrets redacted. GDPR: consent text, retention, controller named.
Rejected alternatives and research: see notes (2026-10-30).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert releases this task (removes the parked label) before any design work
- [ ] #2 Bench measurement of TLS heap peak/steady state on a PSRAM board recorded in notes; go/no-go decided
- [ ] #3 Proposed ADR (outbound TLS client on PSRAM boards, recording format) accepted by Robert
- [ ] #4 OT frames carry a microsecond reception timestamp set at enqueue; a recorded day replays with the original timing (old fixed-750 ms vs new paced, demonstrated)
- [ ] #5 Debug and OT streams upload continuously to an R2 EU bucket via the Worker; 24 h bench run shows no loop gap above normal and the real volume per stream
- [ ] #6 With outbound internet blocked the upload fails quietly: no loop stall, no reboot, at most one log line per retry interval (capture transcript)
- [ ] #7 Opt-in off by default, consent text in the web UI, secrets redacted, retention lifecycle rule set on the bucket
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
<!-- SECTION:NOTES:END -->
