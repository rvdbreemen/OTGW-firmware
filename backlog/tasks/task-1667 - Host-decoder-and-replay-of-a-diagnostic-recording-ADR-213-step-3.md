---
id: TASK-1667
title: Host decoder and replay of a diagnostic recording (ADR-213 step 3)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T20:35:00Z'
updated_date: '2026-11-07T12:01:00Z'
labels:
  - diagnostics
milestone: 2.0.0
dependencies:
  - TASK-1470
priority: medium
ordinal: 790000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Python decoder for the ADR-213 format (tools/), export of OT frames to the ADR-209 replay format, and a diff between two recordings (TASK-1294 basis). Web UI start/stop/download button in the diagnostics panel.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Decoder round-trips the host-test fixtures and a bench recording
- [ ] #2 A bench recording replays through the existing replay path
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. scripts/diag_decode.py (stdlib): parse the download (file header per diagRecorderStuff.h: OTGWREC magic, version, header length, block size, first/count, ring blocks, boot count, flags, fw version, githash, route names), every 4096 B block (16 B header; lost blocks first_record_seq 0xFFFFFFFF), every record type 1..23 with LEB128 dt and len-u8 texts (255 = cut). Output: summary, --json records with absolute uptime ms, --otframes FILE (ADR-209 timed lines device_time TAB source TAB frame) for POST /api/v2/simulate, --names FILE to resolve topic/setting ids (ADR-214 16-bit FNV-1a of the lowercased name; the header carries no id table). 2. diff subcommand: PIC command and MQTT publish sequences of two recordings, aligned in order. 3. Round-trip fixture from the REAL encoder: test/host/diag_fixture_gen.cpp includes diagRecorderStuff.h, records a known scenario over several blocks incl. repeats, a gap and a cut text, writes the download (diagDownloadHeaderBegin/End + diagDownloadFill) plus the expected decoded lines; tests/test_diag_decode.py compiles it with MSVC (find_vcvars), decodes and compares; a checked-in copy of the fixture keeps the decoder test runnable without MSVC. Red check: a mutant decoder (dt not varint-decoded) must fail. 4. AC1 bench recording and AC2 replay: ask 6b/54 for a unit, record a few minutes, download, decode, export --otframes, upload and replay through /api/v2/simulate. Web UI button (description) is out of scope here: v2.js is frozen until TASK-1684 lands; noted for a follow-up.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host part done: scripts/diag_decode.py (summary, --lines, --json, --otframes ADR-209 export, --names id labels via ADR-214 FNV-1a, multi-download merge by block_seq, diff subcommand for PIC/OT-Direct commands and MQTT publishes). Round-trip proof against the REAL encoder: test/host/diag_fixture_gen.cpp includes diagRecorderStuff.h, records every record type, a cut text (300 B), dt 20000 ms, a dropped record (Gap), 3000 bus events with repeats over 4 blocks, and builds the download like the route (diagDownloadRange, HeaderBegin/End + 2 route names, diagDownloadFill in 1460 B chunks). tests/test_diag_decode.py: 10 tests OK, including fresh MSVC build of the generator decoding to the same 3449 records and uptimes, and byte-identical to the checked-in fixture tests/fixtures/diag/recording-v1.bin (16,457 B); a mutant decoder (dt read as one byte) is caught; otframes lines match the ADR-209 regex and keep every recorded gap; diff finds a changed publish. Note: the file header carries no topic/setting id table (ADR-214 replaced ADR-213's), so names come from --names. Web UI button: out of scope while v2.js is frozen for TASK-1684.
<!-- SECTION:NOTES:END -->
