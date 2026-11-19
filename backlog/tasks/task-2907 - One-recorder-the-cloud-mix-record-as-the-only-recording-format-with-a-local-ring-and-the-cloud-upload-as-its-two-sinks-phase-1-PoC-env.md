---
id: TASK-2907
title: >-
  One recorder: the cloud mix record as the only recording format, with a local
  ring and the cloud upload as its two sinks (phase 1, PoC env)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T22:27:00Z'
updated_date: '2026-11-17T22:39:00Z'
labels:
  - cloud-recording
dependencies: []
priority: low
ordinal: 1009000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15, option A: one way of recording that covers all streams; remote first, minimal volume, simple over complex, local recording without a backend stays possible. Phase 1 (this task, esp32-combo-tlsdyn only): the cloud recorder also records locally into a ring of LZ-coded records (PSRAM 1.5 MB, or 8 KB heap with the ADR-213 start rule), served by the existing /api/v2/diag/recording routes with the same status keys; diagRec stays off in this env; decode_all.py reads a download file. Phase 2 (after the bench run and ADR acceptance): normal builds switch, diagRec code and its tests retire, an ADR supersedes ADR-213's format.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 RecRing.h: ring of records with eviction of the oldest, lookup by seq and a download serialisation; host-tested with random sizes
- [ ] #2 Capture runs while local recording is on or the cloud logs; each closed record goes LZ-coded into the ring and, when the service is up, to the cloud; a cloud outage no longer pauses a local recording
- [ ] #3 The diag recording routes (status/start/stop/download) drive the new ring in the PoC env with the same status keys; local recording resumes at boot as before
- [x] #4 decode_all.py --file decodes a download into the same four logs; tested on a ring image built by the real header
- [x] #5 esp32-combo-tlsdyn builds; evaluate --quick no new FAIL; no new test failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 phase 1 built (2.0.0-poc.8). RecRing.h (pure): entries magic|fmt|len|seq|t0|payload, wrap-around, eviction of the oldest with a count, find by seq, ringFill for a chunked download with a pin on a partly sent entry (lapses after 10 s) so no entry is ever cut. tests/test_rec_ring.py 2/2 (2,000 random entries, ring holds the newest that fit, download in random chunks gives whole entries in seq order byte-identical); a mutant without the pin fails both. CloudRecDevice.h: cloudRecLocalStart/Stop (PSRAM 1.5 MB once, or 8 KB heap with the ADR-213 rule incl. the task stack), cloudRecUpdateActive (capture while local on or cloud up, clock set), cloudRecStoreLocal (two-pass LZ into the ring, as sent), the task stores every closed record locally before the upload; a cloud outage or block no longer stops a local recording (blocked keeps the task alive with no connection). diagRecorderStuff.ino start/stop/boot-resume and restAPI.ino status/download go to the ring under #if HAS_CLOUD_REC with the same status keys (+ format otgwmix1; seq as text, 64-bit); the web UI needs no change. decode_all.py --file reads an OTGWMIX1 download. tests/test_cloudrec_local_file.py: 30 min of harness records through the real lzEncode + RecRing + ringFill into a file: every record back byte for byte, OT frames, SATTICK and events identical. Test suite: the same 46 pre-existing failing files as the clean-HEAD baseline, no new failure (test_cloudrec_policy fixed for the new Slot member). Build esp32-combo-tlsdyn exit 0: RAM 156,820 B, app 2,259,011 B (98.5 %). evaluate --quick 0 FAIL 0 WARN. Bench check 16 added. Phase 2 (normal build, retire diagRec, ADR superseding ADR-213's format) waits for the bench and Robert.

AC 2 and 3 unticked again: the code is built and the parts that run on the host are tested, but the behaviour on a device (capture during a cloud outage, the routes, resume at boot) needs bench check 16.

PR #701 review follow-up: suppress webhook and BLE provisioning secrets in recordings, finish empty downloads and admit one reader, isolate the experimental PlatformIO core, and associate cloud batches with exact firmware builds. Targeted host tests and a PocketBase upgrade with retained data passed.
<!-- SECTION:NOTES:END -->
