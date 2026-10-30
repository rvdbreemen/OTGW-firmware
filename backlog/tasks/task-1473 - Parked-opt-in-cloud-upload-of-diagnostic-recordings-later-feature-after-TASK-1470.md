---
id: TASK-1473
title: >-
  Parked: opt-in cloud upload of diagnostic recordings (later feature, after
  TASK-1470)
status: To Do
assignee: []
created_date: '2026-10-29T12:45:00Z'
updated_date: '2026-10-30T06:12:00Z'
labels:
  - parked
  - needs-maintainer-ok
  - tooling
dependencies: []
priority: low
ordinal: 633000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: local first; the cloud upload from gruijter's proposal (TASK-1469: encrypted batches over plain HTTP to a Cloudflare store, offline key) is parked as a later feature. Not to be picked up until Robert releases it, and not before the local recording (TASK-1470) works. Constraints already recorded: must fail quietly on a LAN with internet blocked (martreides, TASK-1471); no TLS on the device (encrypt the payload instead); account and key ownership undecided.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert releases this task (removes the parked label) before any design work
<!-- AC:END -->

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
