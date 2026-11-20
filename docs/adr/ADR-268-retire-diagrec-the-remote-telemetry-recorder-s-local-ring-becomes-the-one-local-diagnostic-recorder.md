---
id: "ADR-268"
title: "Retire diagRec: the remote telemetry recorder's local ring becomes the one local diagnostic recorder"
status: "Proposed"
date: "2026-11-20"
binding: false
gate: null
documents_shipped: false
verified_in: []
supersedes: []
superseded_by: null
format: "canonical"
topics:
  - "diagnostics"
  - "remote-telemetry"
  - "flash-size"
  - "recorder"
aliases:
  - "retire diagRec"
  - "one recorder, two sinks"
  - "DiagRecorder removal"
  - "local recording from the telemetry ring"
components:
  - "Local diagnostic recording (the recorder's local ring and the /api/v2/diag/recording* routes)"
symbols:
  - "DiagRecorder"
  - "diagRec"
  - "remoteTelemetryLocalStart"
  - "remoteTelemetryLocalFill"
  - "RecRing"
  - "HAS_REMOTE_TELEMETRY"
context_scope: "selective"
---

<!-- markdownlint-disable MD025 -->

# ADR-268 Retire diagRec: the remote telemetry recorder's local ring becomes the one local diagnostic recorder

## Status

Proposed, 2026-11-20. On acceptance this ADR supersedes ADR-213 and ADR-214. `bin/adr supersede` then records that on both sides; until then both stay Accepted and in force.

**Decision Maker:** User: Robert van den Breemen. In the otgw-architect session on 2026-11-20 he wrote: "Laten we diagRec verwijderen, als we nu toch Remote Telemetry ontwikkelen" ("Let's remove diagRec, since we are developing Remote Telemetry anyway"). Release timing, decided by Robert interactively on 2026-11-20 (about 18:25, relayed by the coordinator and otgw-poc-dev): **now, in 2.0.0**, together with size-gate option A1 (99.5 % temporarily until 2026-11-29, with a linked trim task, TASK-2980).

## Status History

```yaml
status_history:
  - date: 2026-11-20
    status: Proposed
    changed_by: "Agent: Claude (otgw-architect)"
    reason: Initial proposal after Robert's decision to remove diagRec; written as a swap with the remote telemetry recorder
    changed_via: adr-kit
```

## Context

Two recorders in the firmware cover the same need.

**diagRec (ADR-213), on dev today.**

- An opt-in local diagnostic recorder (`DiagRecorder`, `src/OTGW-firmware/diagRecorderStuff.ino` and `.h`). It records into a bounded RAM ring and streams the ring as a binary download through `/api/v2/diag/recording*`.
- Its cost in the beta.401/404 app image is about 4-9 KB of flash: about 2.5 KB of visible symbols plus their strings. Sources: the architect's measurement of beta.404 (`docs/research/2026-11-20-flash-budget-step1-measurement.md` and its follow-up) and the coordinator's measurement.
- It is in use today. `scripts/otgw_capture.py:2559-2560` (TASK-1964) downloads `/api/v2/diag/recording` in every tester capture, and `scripts/diag_decode.py` reads that format.

**The remote telemetry recorder (TASK-2901 PoC, TASK-2937 product feature for 2.1.0), on lane-5e-adr in `wt-5e-adr` at 937d23058.**

- One recorder with two sinks: an outbound upload, and a local ring of the same records.
  - **The ring.** `src/OTGW-firmware/RecRing.h:1-17`: each entry is magic 0xA5, fmt (4 plain, 5 LZ), len, seq, t0 and payload. The download file is "OTGWMIX1" plus a version and a header with the firmware, githash and uid. When the ring is full it drops the oldest entries and counts them.
  - **Ring size.** `RemoteTelemetryDevice.h:1094-1095`: 1536 KB in PSRAM (`platformPsramAllocOnce`), and 8 KB of heap on a board without PSRAM.
  - **The routes.** In that environment the existing `/api/v2/diag/recording*` routes already drive the new ring: `diagRecorderStuff.ino:60-70` behind `#if HAS_REMOTE_TELEMETRY`, and `restAPI.ino:3755` and `:3854-3914`.
- The lane's design already says the retirement of diagRec follows in the normal build, with a separate ADR replacing ADR-213's format (ADR-290 point 7, now folded into that lane's ADR-218).

**Why both cannot stay.**

- The app slot is tight: beta.401 is at 93.9 % of 2,293,760 B, and the lane with the recorder is at 98.8 %. The TASK-1993 gate fails at 98 %.
- Keeping two recorders costs flash for no extra function.
- Two formats need two decoders.

## Decision

The firmware keeps one local diagnostic recorder: the local ring of the remote telemetry recorder. `diagRec` (`DiagRecorder` and its ADR-213 format) is removed in the same change that brings the recorder into the normal build. It is never removed before its replacement is in.

0. **Release:** 2.0.0. The recorder's local sink enters the normal combo build in 2.0.0, and diagRec leaves it in the same change. `DiagRecorder` and its ADR-213/ADR-214 code are removed in full: the source is deleted, not compiled out. Robert confirmed this on 2026-11-20: "Verwijder de diagRec uit 2.0.0" ("Remove diagRec from 2.0.0"), plus DM "A1", relayed by the coordinator.
1. **The routes stay.** `/api/v2/diag/recording*` (status, start, stop, download) keeps its paths and status keys, and is served from the recorder's local ring. That is what the lane's `#if HAS_REMOTE_TELEMETRY` branch already does.
2. **The download format changes** from the ADR-213 format to the "OTGWMIX1" file of `RecRing.h`. `scripts/otgw_capture.py` and `scripts/diag_decode.py` read the new format through the lane's decoder (`tools/remote-telemetry/decode_all.py`, `read_mix_file`). A capture tool that meets an unknown format or a 404 skips the recording and keeps capturing everything else.
3. **The flash and RAM diagRec frees go to the recorder in the normal build.** This ADR does not set the size gate. Robert's A1 (99.5 % until 2026-11-29, then back to 98 %, with TASK-2980) is a separate TASK-1993 decision.
4. **Local recording keeps working without a backend and without TLS.** On a board without PSRAM the ring is 8 KB of heap, refused when the heap is too low (ADR-030), as in the lane today.

## Decision Contract

### Must

- Remove `DiagRecorder` / `diagRec` only in the commit (or in the same pull of commits) that makes the remote telemetry recorder's local sink part of the normal build.
- Keep the `/api/v2/diag/recording*` paths and status keys working on every build that has a recorder.
- Make `otgw_capture.py` read the new download format, or skip a recording it cannot decode, in the same change.
- Keep ADR-213's redaction of secrets in what the new recorder stores (ADR-290 / lane ADR-218: "secrets never leave the gateway").

### Must Not

- Must not ship a release in which neither diagRec nor the recorder's local sink is present, unless Robert has explicitly chosen that.
- Must not keep the gate above 98 % after 2026-11-29 because of this change: that end date belongs to Robert's A1 decision (TASK-2980).

### Exceptions

- None.

### Verification

- The host test `tests/test_rec_ring.py` on the lane compiles `RecRing.h` as it is.
- A capture-tool test reads a fixture download in the "OTGWMIX1" format and skips an unknown one without aborting.
- A build check: `DiagRecorder` symbols are absent from the normal-build ELF (`nm`), and the build log shows the freed bytes.
- Bench: a tester capture with `otgw_capture.py` on .61 (no PSRAM, 8 KB ring) and .63 (PSRAM) contains a decodable recording.

## Alternatives Considered

- **Remove diagRec in 2.0.0 without bringing in the recorder's sink.** It frees 4-9 KB at once. Rejected: 2.0.0 stable would have no local diagnostic recording, and the tester capture tool would lose a data source.
- **Swap in 2.1.0, with remote telemetry (TASK-2937).** This was the architect's recommendation. Robert chose 2.0.0 instead, so the recorder and the temporary 99.5 % gate enter the stable.
- **Keep both recorders.** Rejected: two recorders, two formats and two decoders for one need, and flash the recorder needs.
- **Keep diagRec and do not build the recorder's local sink.** Rejected: Robert asked for one remote telemetry recorder with a local sink (lane decision of 2026-11-15, option A), and a local-only recorder already exists inside it.
- **Raise the size gate instead of freeing room.** Rejected here: that is a separate TASK-1993 question to Robert. At 99.5 % about 11.5 KB would remain, 3-4 days of today's growth.

## Consequences

**Positive:**

- About 4-9 KB of flash, plus diagRec's RAM ring when enabled, goes to the recorder.
- One recorder, one format and one decoder for local downloads and uploads.

**Negative:**

- The download format changes. Old ADR-213 recordings need the old decoder, which stays in git history. Mitigation: the capture tool and `diag_decode.py` move to the new reader in the same change, and an unknown format is skipped, not fatal.
- The swap alone does not solve the size gate. Recorder plus TLS add about 113 KB (lane 1a0ea63: 2,266,576 B, 98.8 %), against the 18.7 KB still needed under the 98 % gate. Mitigation: the 2.1.0 levers (TASK-2981) and, if needed, the mbedTLS trim, as weighed in the joint proposal to Robert.

## Open Questions

- [x] Release timing (Robert): (A) with the recorder in 2.1.0, the architect's recommendation, or (B) now in 2.0.0. **Answered 2026-11-20 by Robert (interactive, relayed by the coordinator):** now, in 2.0.0, with the recorder's local sink in the same change (so not B's "without local recording"), and size gate A1 99.5 % until 2026-11-29 with TASK-2980.
- [ ] Is the 8 KB heap ring on a board without PSRAM enough for a useful tester capture (minutes of records)? Measure one capture on .61 with the recorder before the swap lands.

## Related Decisions

- **ADR-213 (Record a local diagnostic event stream in a bounded on-device ring)** and **ADR-214 (16-bit FNV-1a hash ids, amends ADR-213)**: both superseded by this ADR when it is Accepted. Their redaction rules carry over; the recorder's own event ids replace ADR-214's hash.
- **ADR-030 (heap thresholds)**: the 8 KB heap ring is refused below the minimum free heap, unchanged.
- **The telemetry lane's ADR-218 / ADR-263 (outbound TLS and the remote telemetry recorder, lane-5e-adr)**: define the recorder and its local sink; this ADR only retires diagRec in their favour.
- **TASK-1993 / ADR-191 (size gate, app slot)**: unchanged by this ADR.

## References

- Robert's decision: otgw-architect session, 2026-11-20.
- Lane facts in `wt-5e-adr` at 937d23058:
  - `src/OTGW-firmware/RecRing.h:1-17`;
  - `src/OTGW-firmware/RemoteTelemetryDevice.h:1094-1095` and `:1218-1220`;
  - `src/OTGW-firmware/diagRecorderStuff.ino:60-70`;
  - `src/libraries/Platform/src/boards.h:328-333` (`HAS_REMOTE_TELEMETRY`);
  - `tools/remote-telemetry/decode_all.py`.
- Dev: `scripts/otgw_capture.py:2559-2560`, `scripts/diag_decode.py`.
- Flash measurements:
  - `docs/research/2026-11-20-flash-budget-step1-measurement.md`;
  - TASK-1955, TASK-2980, TASK-2981;
  - otgw-poc-dev's lane measurement (1a0ea63: 2,266,576 B).
- Tasks: TASK-2937 (remote telemetry), TASK-1964 (capture tool downloads the recording).
