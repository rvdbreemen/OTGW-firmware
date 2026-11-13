---
id: TASK-1871
title: >-
  REST /api/v2/otgw/messages/<id>: tell never seen from unknown or rejected
  (status and last-seen)
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-12T18:20:00Z'
updated_date: '2026-11-13T11:27:00Z'
labels:
  - feature
  - rest-api
milestone: 2.0.0
dependencies: []
ordinal: 946000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reporter thaghostnl (Discord 285886063266889728), #alpha-testing https://discord.com/channels/812969634638725140/1514720723980259460/1570499415284781151 (2026-11-12 18:03:59 UTC). Setup: 2.0.0-beta.170+d260e69, PIC gateway 6.8, OTGW Classic Combo (ESP32-S3), Remeha Avanta 23C + iSense. Observation: GET /api/v2/otgw/messages/10 returns {"label":"TSP","value":0,"unit":""} both before ID 10 was ever requested and after the boiler answered BF00A0000 Unknown-Data-Id (after PM=10); only the live log tells them apart. Suggestion: add a status per ID (never_seen / ack / unknown_dataid / data_invalid) and a last-seen timestamp to the response. Existing: GET /api/v2/otgw/boiler-support (restAPI.ino:787) already lists unsupported_read / unsupported_write from the in-RAM bitmaps; never-seen vs seen and last-seen are missing. Tester feedback: Robert decides (needs-maintainer-ok). Evidence: log excerpt in the post; feature request, no capture needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Only genuine boiler answers count, the same boilerEvidence rule as the boiler-support bitmaps (B frames; no bLocalAnswer, no replayed line), on both the PIC and the OT-Direct path
- [x] #2 Host test on the real recording and status functions sliced from the firmware: red on the old code, green on the fix; covers never seen, ack, unknown_dataid, data_invalid, a later ack overriding unknown, and a gateway-made answer not counting
- [ ] #3 docs/api/openapi.yaml and the REST docs describe the two fields; evaluate --quick no new FAIL; esp32-combo build; bench on .61 (and .63 PIC if 09 chooses): an ID never requested shows never_seen/0, an acked ID shows ack with a rising last_seen, and an Unknown-DataID ID (e.g. after PM=) shows unknown_dataid
- [x] #4 GET /api/v2/otgw/messages/<id> (and /otgw/id/<id>, /otgw/label/<label>) adds status (never_seen / ack / unknown_dataid / data_invalid) and last_seen_age_s (seconds since that boiler answer, -1 when never_seen) after label/value/unit; existing fields unchanged
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Plan (ba, 2026-11-12). Today: sendOTValue (restAPI.ino:2933) returns label/value/unit from OTcurrentSystemState; nothing records per ID whether or when the boiler answered. The boiler-support bitmaps (OTGW-Core.ino:5599-5606) record acked/unsupported per ID, but persisted across boots and without a time, and DATA_INVALID is not tracked. 1) Storage, OTGW-Core.ino next to those bitmaps: uint32_t boilerLastSeenMs[OT_MSGID_MAX+1] and uint8_t boilerLastRespType[OT_MSGID_MAX+1] (134 x 5 = 670 B RAM, this boot only, not persisted). 2) One recorder, recordBoilerAnswer(id, type, nowMs), called in the existing B-frame branch (OTGW-Core.ino:5956-6001) under the same boilerEvidence rule, for READ_ACK, WRITE_ACK, DATA_INVALID and UNKNOWN_DATA_ID; millis 0 is stored as 1 so 0 keeps meaning never. 3) boilerMsgStatus(id) maps it: 0 -> never_seen, READ/WRITE_ACK -> ack, UNKNOWN_DATA_ID -> unknown_dataid, DATA_INVALID -> data_invalid; the latest answer wins. 4) sendOTValue and sendOTLabel emit status and last_seen (uptime seconds of the last answer, 0 = not seen this boot) after unit; JsonEmit, no ArduinoJson, existing fields unchanged. 5) Writer is the loop task, reader the async_tcp REST task: aligned 32-bit and 8-bit stores are atomic on the S3; a torn pair (new time, old type) is harmless for one response. 6) Host test slices the real recorder and status functions; red on old code (no functions), green on fix; static check that sendOTValue emits both fields. openapi.yaml + REST docs updated. Bump, esp32-combo build behind 219/220, bench by 09. Open choice for Robert: last_seen as uptime seconds (works without NTP; 0 = never) vs Unix epoch (readable, but 0 also while the clock is unset). Recommendation: uptime seconds, plus nothing else (age = uptime now - last_seen is one subtraction for the client). Not in scope: the 1.x line, the thermostat side, persisting across reboot.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-12 Robert approved option A (DM 1570541077226717255, relayed by the coordinator): per ID both a last-seen (0 = never seen) and a status (never_seen / ack / unknown_dataid / data_invalid), reusing what /otgw/boiler-support already tracks. needs-maintainer-ok removed. Queued after TASK-1872; needs a firmware build (behind 219 and 220).

2026-11-12 field decision (coordinator, relaying for Robert's A; ba had asked uptime vs epoch): report the AGE, last_seen_age_s = seconds since the last boiler answer for that ID, -1 when status is never_seen. Reason: 0/never stays unambiguous through status, the client needs neither uptime nor NTP, it cannot be confused with an unset clock, and it matches thermostat_age_s / boiler_age_s in /api/v2/health (-1 = never, restAPI.ino:3621-3622). Internally a uint32 millis per ID plus the uint8 answer type (type 0 = never), 670 B RAM, this boot only. Name differs from Robert's word 'last-seen'; meaning kept.

2026-11-12 implementation: OTGW-Core.ino recordBoilerAnswer / boilerAnswerAgeS / boilerAnswerStatus between 'begin/end boiler-answer record' markers, one call in processOT's B-frame branch under boilerEvidence; restAPI.ino sendOTValue and sendOTLabel emit status (FPSTR) and last_seen_age_s after unit; declarations in OTGW-firmware.h; docs/api/openapi.yaml and docs/api/README.md. Tests: new tests/test_boiler_answer_status.py (9 functional cases on the sliced real block, 2 wiring checks): on origin/dev 789dc56b5 setUpClass errors (markers not found) and both wiring checks fail; on the fix 11/11 OK. It caught a real bug in the first version: storing millis 0 as 1 made age at now=0 wrap to 4294967 s; fixed by using the answer type as the seen flag. tests/test_boiler_support_bitmap_b_only.py (slices the real processOT branch) got a recorder stub and a new case: only genuine B frames record (B26 ack, B10 unknown, B1 write-ack), not the PIC-built A57 nor the gateway-made B6 (local): 9/9 OK.

2026-11-13 rebase onto f1b2731fd. Full tests folder (229 files, tests/ + test/host/, one file per run_tests call, commit < 82 %): 227 green first pass; test_boiler_unsupported_origin and test_local_frame_consumers failed to compile (processOT now calls recordBoilerAnswer, outside their slice). Fix: a no-op recordBoilerAnswer double in both harness .cpp files, next to their other consumer doubles; moving the block into the sliced range broke test_ot_support_migration, so reverted. Rerun via run_tests: both 1/1 green, test_ot_support_migration green, test_boiler_answer_status 11/11, test_boiler_support_bitmap_b_only green. evaluate.py --quick: 85 PASS 0 WARN 0 FAIL. Bump beta.221 -> beta.222.

2026-11-13 bench beta.222+a5e61eb by otgw-bench-tester 09 (relayed by coordinator): PASS never_seen/-1 with label/value/unit unchanged; PASS .61 OT-Direct loopback: gateway-made answers do not count; PASS ID>127 (128 reserved, 400 unchanged); no extra reboots, crashlog empty, settings diff none on both units. NOT testable on the bench (no boiler): ack with rising age, unknown_dataid after PM=, data_invalid -> host test only (test_boiler_answer_status 11/11, red on f1b2731fd), field validation open. AC3 stays open for that field part. AC1: host test (gateway answer not counting) + .61 bench; PIC path by host test only. Pushed a5e61ebd7 to origin/dev (f1b2731fd..a5e61ebd7). Posted zip to #alpha-testing, Discord message 1570761730533883964.

AC4: /messages/<id> checked on the bench; /otgw/id/<id> and /otgw/label/<label> share sendOTValue/sendOTLabel, both checked on the source by test_boiler_answer_status (wiring test), not separately on the bench. Status In Review: AC3 waits on field validation with a real boiler (ack age, unknown_dataid).
<!-- SECTION:NOTES:END -->
