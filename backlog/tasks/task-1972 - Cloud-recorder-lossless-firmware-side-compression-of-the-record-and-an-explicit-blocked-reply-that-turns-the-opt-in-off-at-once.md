---
id: TASK-1972
title: >-
  Cloud recorder: lossless firmware-side compression of the record, and an
  explicit blocked reply that turns the opt-in off at once
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T13:40:00Z'
updated_date: '2026-11-15T14:05:00Z'
labels:
  - cloud-recording
dependencies: []
priority: low
ordinal: 1006000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15: 'Zorg voor maximale data compressie aan de kant van de firmware. Er mag echter geen data verloren gaan en het moet niet overdreven complex worden. Als de firmware geblokkeerd wordt door de backend onmiddellijk de opt in uitschakelen response geven naar het device. Plus de verbinding verder weigeren.' Research branch only (TASK-1963 PoC).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Backend: every request of a blocked gateway (login, refresh, batch create) gets HTTP 403 with an explicit blocked message; tested against local PocketBase, with a mutant
- [ ] #2 Firmware: a 403 blocked reply turns the opt-in off at once and stops all further connections
- [x] #3 Compression variants measured on realistic records (size per stream, RAM, code size estimate); choice recorded with the numbers
- [x] #4 Chosen compression implemented lossless: host decoder gives every stream back byte-identical; record never larger than without compression
- [x] #5 esp32-combo-tlsdyn build compiles; evaluate --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 evidence. Blocked reply: pb_hooks/blocked.pb.js throws 403 'Gateway blocked (otgw-blocked): telemetry opt-in off' on password login, token refresh, own record update and batch create (the batches rule no longer carries the blocked clause: PocketBase checks the rule before the create hook and would answer a bare 400). PocketBase capitalises the first letter of the message, so the marker sits mid-message. test_client.py check 9 (login, upload with the pre-block token, refresh, own update all 403 + marker) PASS, 1-10 PASS; without blocked.pb.js check 9 FAILs (200 200 200 200). Firmware: crec::classifyReply() maps 403 + 'otgw-blocked' to OUT_BLOCKED (tests/test_cloudrec_policy.py 4/4, mutant on 401 fails); login and upload read the body; cloudRecBlocked() writes enabled=0 and the upload task ends with platformTaskExit(); the auth-refresh check after a 400 is gone. Compression: compress_study.py (10 min, poc.2 image, random argument values), share of raw record bytes: light debug 83.5 % uniform / 71.8 % zipf, full debug 81.0 % / 65.2 %; zlib-9 82.9 / 71.8 / 80.5 / 64.7 % (does not fit without PSRAM); LZSS 96-74 %; order-0 RC 93-88 %; order-1/2 hashed worse than order 0. A 16-slot call-site cache saved 2-7 % more on top of LZ at the cost of a second debug format: left out. Chosen: LZ-RC (crec::lzEncode, 3.2 KB static, fmt 5, sent only when smaller; two passes: count then stream base64 to TLS, no second buffer). tests/test_cloudrec_lz.py: C bytes == otrec.lz_encode on 42 harness records + 10 edge cases, otrec.lz_decode gives every input back, counting sink exact; mutant hash constant fails. proof_all_streams.py sends LZ records like the device: no-psram 30 min 49 records, 85.0 % sent, OT/SATTICK/debug identical; psram profile identical (already zlib, LZ not used). Build esp32-combo-tlsdyn 2.0.0-poc.3 exit 0: RAM 155,724 B (+3,264), app 2,271,315 B (99.0 %, 22.4 KB left), no CloudRec warnings. evaluate --quick 0 FAIL 0 WARN. AC 2 open: runtime on a device (bench-tester), with TASK-1971 AC 3.
<!-- SECTION:NOTES:END -->
