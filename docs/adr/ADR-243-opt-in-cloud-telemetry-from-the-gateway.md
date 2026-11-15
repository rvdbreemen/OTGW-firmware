---
id: "ADR-243"
title: "Opt-in cloud telemetry from the gateway: outbound TLS to a self-hosted collection point, one compressed record a minute, bridge commands included"
status: "Accepted"
date: "2026-11-15"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
topics:
  - "diagnostics"
  - "recording"
  - "telemetry"
  - "tls"
  - "privacy"
  - "compression"
aliases:
  - "cloud recorder"
  - "cloud telemetry"
  - "TASK-1473"
components:
  - "cloud recorder (CloudRec*.h)"
  - "PocketBase collection point (tools/cloud-recorder/pocketbase)"
symbols:
  - "cloudRecSetup"
  - "crec::Recorder"
  - "crec::lzEncode"
  - "cloudRecEvBridge"
context_scope: "selective"
format: "canonical"
---

<!-- markdownlint-disable MD025 -->

# ADR-243: Opt-in cloud telemetry from the gateway

## Status

Accepted (2026-11-15, Robert: "Akkoord met ADR 243"). Guideline-level per ADR-080: it governs a proof of concept that exists only in the
`esp32-combo-tlsdyn` build env (`HAS_CLOUD_REC`) on branch `research/cloud-recording`; no normal build contains
it. Host tests cover the format and the policy (`tests/test_cloudrec_codec.py`, `tests/test_cloudrec_lz.py`,
`tests/test_cloudrec_policy.py`, `tools/cloud-recorder/pocketbase/test_client.py`); the device behaviour waits for
the bench (`docs/diagnostics/cloudrec-bench-job.md`).

## Status History

- 2026-11-15: Accepted by the maintainer. The device-side evidence (bench job, 16 checks) is still to come; a bench
  result that contradicts this decision is a reason to supersede it.
- 2026-11-15: Proposed (TASK-1473, TASK-1991, built on TASK-1862, TASK-1938, TASK-1963, TASK-1971, TASK-1972, TASK-1974,
  TASK-1979).

## Context

The maintainer wants a capture-like recording that needs no separate script: what the firmware, the boiler, the
thermostat and SAT did, after the fact, from gateways whose owners opt in. Constraints set by the maintainer
(2026-11-14/15): free, data in the EU and self-hosted (PocketBase on his Synology behind a Cloudflare Tunnel),
minimal service-side logic, secure from the device itself also on boards without PSRAM, lossless, every gateway's
data deleted after 30 days, central control per gateway (block, telemetry mode).

ADR-003 keeps the firmware's web server on plain HTTP; ADR-218 (Proposed) allows outbound TLS client requests on
boards with PSRAM only, because the stock mbedTLS record buffers do not fit a board without PSRAM. ADR-213 records a local diagnostic event stream and leaves port 25238 bridge
traffic out.

## Decision

1. **Outbound TLS client only, for this one upload.** The web server stays HTTP (ADR-003 unchanged for every
   server side). The upload uses esp-tls with session tickets against four pinned roots (GTS R1/R4, ISRG X1/X2),
   on mbedTLS rebuilt with dynamic buffers (`CONFIG_MBEDTLS_DYNAMIC_BUFFER`, DTLS off), so that a board without
   PSRAM may hold a handshake too: this goes beyond ADR-218's PSRAM-only scope, for this PoC env only, and stays to
   be proven on the bench.
2. **Collection point: PocketBase, self-hosted.** Collections `devices` (the register: uid, blocked, telemetry
   mode) and `batches`. Logic only in collection rules plus two hooks: retention (every record older than 30 days
   deleted, hourly, not configurable) and the blocked reply (HTTP 403 `otgw-blocked` on every request of a
   blocked gateway).
3. **Opt-in and identity.** Off unless `/cloudrec.cfg` says `enabled=1`. A gateway enrolls itself with
   `otgw-<MAC>` and a secret kept in NVS. A blocked gateway turns its own opt-in off and stops connecting.
4. **One record a minute, or earlier when a section is full.** Sections: OT frames (range-coded, about 1 B per
   frame), SATTICK, debug output (address tokens), events (everything else the firmware receives, sends or measures),
   lost counts. The whole record goes LZ + range-coded with a CRC-32 when that is smaller (fmt 5), else as it is
   (fmt 4). Every loss that cannot be avoided (recorder busy, queue full) is counted in the next record.
5. **Events cover the firmware's inputs, outputs and sensors**: PIC and OTDirect commands, PIC responses, MQTT in
   and out, MQTT connection, REST requests (bodies redacted as ADR-213; GETs answered 200 counted per route per
   minute), weather, BLE, Dallas, S0, WiFi, settings (redacted), time sync, reboot, telnet keys, webhook, PIC
   flash, OTA and file events, health every minute, simulation and telemetry-mode changes.
6. **Port 25238 bridge commands are recorded by this cloud recorder, inbound only, as text** (maintainer decision
   2026-11-15). This narrows ADR-213's bridge exclusion for this recorder only: the local recorder of ADR-213 keeps
   leaving bridge traffic out, and the outbound PIC stream stays untouched (ADR-177). A line the ADR-217 SAT filter
   drops is recorded as refused.
7. **One recorder, two sinks** (maintainer decision 2026-11-15, option A): the closed records also go to a local
   ring (PSRAM 1.5 MB or 8 KB heap), so local recording without a backend keeps working and both sinks carry the
   same records; one decoder reads a download and the collection point. In the PoC env the ADR-213 routes and
   switch drive this ring; the normal build and the retirement of `diagRec` follow after the bench (a separate
   ADR then supersedes ADR-213's format).
8. **Telemetry mode per gateway**, set in its record by the maintainer only: default, full, full+trace or custom
   log flags, with an end time after which the gateway returns to default by itself; never the simulation
   switches.
9. **Accepted exposure:** TLS ends at the Cloudflare edge, so Cloudflare can read the payload (maintainer
   decision 2026-11-15, no device-side encryption).

## Decision Contract

- No capture point blocks for more than 10 ms; a busy recorder costs a counted loss, never a stall.
- Secrets never leave the gateway (ADR-213 redaction helpers).
- A gateway can set none of the maintainer's fields (blocked, mode, flags, mode_until, mode_note).
- Nothing of this is compiled without `HAS_CLOUD_REC`; every call site sits inside `#if HAS_CLOUD_REC`.

## Alternatives Considered

- Cloudflare Worker + R2 (the original TASK-1473 plan): rejected for the per-request cost model and vendor logic;
  the self-hosted PocketBase needs no code beyond rules and two hooks.
- Plain HTTP to the NAS on the LAN only: rejected, the maintainer wants it secure from the device.
- A reverse proxy that adds TLS: rejected by the maintainer (no proxies).
- zlib on the device: the compressor state does not fit without PSRAM; LZ-RC reaches zlib-9 within 0.5 point.
- The local ADR-213 ring as the cloud's event source: rejected for now; it would change ADR-213's tested format
  and its repeat caches are per block, not per cloud record.

## Consequences

- About 2 to 3 MB of payload per gateway per day without debug text; storage at most 30 days.
- The PoC env cannot use LTO and uses 98.5 % of the app partition.
- The hybrid compile overwrites the shared Arduino libs package on the build machine (see the tools README).
- Not verified on a device yet (bench job).

## Open Questions

- The device results of the bench job (TLS heap peak without PSRAM, resumption, real compression).
- Whether to promote any of this out of the PoC env (a separate decision).

## Related Decisions

- ADR-003 (HTTP only, server side unchanged), ADR-218 (outbound TLS on PSRAM boards; this PoC goes further), ADR-213 (local recorder; bridge exclusion narrowed for this recorder
  only), ADR-217 (bridge SAT filter), ADR-177 (verbatim PIC stream), ADR-218 (TLS without PSRAM), ADR-080.

## References

- `docs/diagnostics/cloud-telemetry-completeness.md`, `docs/diagnostics/ot-stream-encoding-study.md`,
  `tools/cloud-recorder/README.md`, `tools/cloud-recorder/pocketbase/README.md`.
