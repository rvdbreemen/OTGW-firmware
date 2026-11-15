---
id: TASK-1971
title: >-
  Cloud recorder: self-enrollment by unique id, block list, fixed 30-day
  retention, backoff and pause when the service is down
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T13:17:00Z'
updated_date: '2026-11-15T13:36:00Z'
labels:
  - cloud-recording
dependencies: []
priority: low
ordinal: 1005000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-15. Every gateway enrolls itself with a unique id (from the MAC) on first contact; the devices collection is the register of gateways that log. A new gateway may log by default; a gateway Robert marks blocked turns its opt-in telemetry off. All data of every gateway is deleted after 30 days, always. When the service is down telemetry stays enabled, capture pauses, and the gateway retries with a backoff from 5 min up to 24 h. Service-side logic minimal (PocketBase rules, one cron). Research branch only (TASK-1963 PoC).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Backend: self-registration by uid (unique, pattern-checked), device cannot set or clear blocked, blocked device cannot create batches; tested in local PocketBase
- [x] #2 Backend: retention fixed at 30 days, not configurable, runs hourly; request logs kept at most 7 days
- [ ] #3 Firmware: enroll/login by uid with a per-device secret in NVS, reads blocked from the auth record and turns telemetry off (persisted) when blocked
- [x] #4 Firmware: capture paused while the service is unreachable; backoff 5 min doubling to 24 h with jitter, reset on success; schedule host-tested against the real header
- [x] #5 esp32-combo-tlsdyn build compiles
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 evidence. Backend (local PocketBase 0.40.6, Docker): test_client.py checks 1-10 PASS (enroll+login by uid, dup uid refused, enroll with blocked refused, bad uid pattern refused, own label/fw PATCH ok but blocked/uid refused, batch stored + resend refused, foreign device refused, no list, block -> login shows blocked + batches refused, retention cron deletes a backdated batch and keeps a fresh one); check 11 fails on the LAN as designed (tunnel path rule). Mutant migration without the two blocked rules: checks 3 and 9 FAIL. Extra: the firmware's enroll body, own PATCH and auth-refresh after a block (raw JSON has "blocked":true, compact) checked by hand. Retention fixed 30 days, hourly; logs.maxDays 7; devices:create 3/min per IP. Firmware: CloudRecPolicy.h backoff 5 min doubling to 24 h, +0-20 % jitter, reset on success; tests/test_cloudrec_policy.py 3/3 OK, a mutant with a 12 h cap fails 2 tests. CloudRecDevice.h: uid otgw-<MAC>, 128-bit secret in NVS, enroll on unknown uid, PATCH label/fw/board once per firmware, blocked -> enabled=0 written to /cloudrec.cfg, capture paused until a non-blocked login and while the service is down, network down pauses without a backoff step. ./build.sh --target esp32-combo-tlsdyn exit 0 (2.0.0-poc.2): RAM 152,460 B (-64 B), app 2,270,307 B of 2,293,760 (99.0 %, +2,840 B), no warnings in CloudRec files. evaluate.py --quick: 98 checks, 0 FAIL, 0 WARN. tests/test_cloudrec_codec.py OK. AC 3 left open: the firmware side runs on a device only (bench-tester, with TASK-1963 AC 2-3).

2026-11-15 host scripts moved to uid: decode_all.py and export_batches.py take <device-uid>, simulate_gateway.py enrolls on an unknown uid, batch_size_study.py creates by uid. Round trip simulate -> export on the new schema: 3,920 frames back, 1 gap line. proof_all_streams.py (image = the poc.2 tlsdyn build), 30 min: no-psram 49 records and psram 30 records, OT, SATTICK and debug bytes identical. Two proof-generator fixes: a record split mid-second now carries the current heap item (the firmware does this with dbgHeap after begin), and formats with a * width are skipped (the device sends those as line raw).
<!-- SECTION:NOTES:END -->
