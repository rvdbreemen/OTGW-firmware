---
id: TASK-865
title: 'EPIC: 2.0.0 ESP32-S3-only async + FreeRTOS migration (ADR-123/127/128)'
status: To Do
assignee: []
created_date: '2026-06-13 05:37'
updated_date: '2026-06-28 21:20'
labels:
  - async-esp32s3
  - epic
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tracking epic for migrating the OTGW-firmware 2.0.0 line to an ESP32-S3-only, maximally-async + FreeRTOS firmware (EMS-ESP32 blueprint). Parents the 13 implementation tasks across ADR-123's four phases.

## Decisions (Accepted 2026-06-12)
- ADR-123: hybrid concurrency model — dedicated FreeRTOS PIC-UART task + event-driven async networking (ESP32Async AsyncTCP / ESPAsyncWebServer + AsyncWebSocket, espMqttClient).
- ADR-128: drop ESP8266; supersedes ADR-082. ESP32-S3 only.
- ADR-127: combo single ESP32-S3 binary, runtime PIC/OTDirect boot detection.

## Phase map -> tasks
- drop-esp8266: seq 1, 2, 3
- foundation: seq 4, 5
- phase1-pic: seq 6
- phase2-mqtt: seq 7, 8
- phase3-web/ws: seq 9, 10, 11
- phase4-cleanup: seq 12, 13

## Cross-cutting field-validation gate (epic-level; no single task closes it)
Full-stack ESP32-S3 hardware soak after seq 6-11 land: OTGW32 (esp32) + esp32-classic (S3 mini in Classic socket, PIC) + esp32-combo dual-boot, validated in #dev-sat-mqtt. The project has NO automated firmware test harness; concurrency correctness is field-only.

## Branch
feature-2.0.0-esp32s3-only (the new active 2.0.0 mainline), worked via the feature-2.0.0-esp32s3-async worktree.

## Refs
docs/adr/ADR-123, ADR-128, ADR-127; docs/research/2.0.0-async-modernization.md; other-projects/EMS-ESP32-dev (blueprint).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
2.0.0 FINISH PLAN (2026-06-28). Core migration phases 1-13 DONE. Remaining = 4 workstreams by what unblocks them. Critical path: WS-B decisions -> WS-A code -> WS-C field soak. WS-D descoped.

WS-A IMPLEMENTABLE NOW (no hardware) - drain back-to-back:
- 936 parity: POST /api/v2/mqtt/republish (AC#1 code-ready), WiFi-reconnect rebind guard (AC#2 bench), dropped-set-command trace (AC#3). AC#4 done.
- 935-sat: expose advanced OT-Direct/SAT settings over REST v2 (Settings Phase2) - NOT STARTED, ~50 keys GET/POST + v2.js SET_META. Largest remaining code task. Needs WS-B-1 scope first.
- 925 Phase1: satsource whitelist, PATCH/DELETE auth, comment scrubs (AC#1). Phase2 done.
- 940 SoftAP AP_FALLBACK passphrase too short (otgw123). Already modified in tree.
- 934 AC#5 runtime capture -> folds into WS-C soak.
NOTE: duplicate task-935 id collision (heap-soak vs sat-settings). Renumber one.

WS-B MAINTAINER DECISIONS (unblock then code):
- B-1 935-sat: which advanced keys in scope? Rec: SAT tuning set used in #dev-sat-mqtt; defer obscure.
- B-2 871: F1 per-drip-cycle full device-block on 52 entities - tighten to 1/pass? Rec: tighten.
- B-3 802: SAT sim F7 emulator path A(debug force-boiler-present hook)/B(serial PIC emu)/C(park). Rec: A.
- B-4 891.4: 24h window per-cycle ring vs aggregates under 40k heap. Rec: measure then aggregates.
- B-5 892: hybrid HP+boiler notification channel. Low prio, may slip 2.0.0.
- B-6 v2 default: flip settings.ui.bUseV2=true default after WS-C visual sign-off.

WS-C FIELD-VALIDATION CAMPAIGN (the real gate; one coordinated hardware effort):
- C-1 combined soak >=1h bootcount delta 0: closes 879 AC#4, 865.14 AC#5, 865.15 AC#7, 865.16 AC#6, 865.17 AC#5, 865.18 AC#4.
- C-2 heap-frag soak 24-72h: 935-heap + 934 AC#5 + 779/897. Proves/kills ESP32-S3 gating -> Phase3 removal. Needs free S3.
- C-3 HA discovery+climate: 942 AC#6, 939, 943 AC#5. Pair Honeywell Heat/Cool reporter.
- C-4 v2 visual+a11y: 933 AC#4/6, 899, 896, 887. Live OT bus device.
- C-5 SAT field: 809 (null while active?), 930 AC#6 (needs stock Mijia).
RESOURCE CONFLICT: C-2 and C-4 share the one free S3. Run C-4 first (short), free board, then C-2 (long unattended).

WS-D DESCOPE FROM 2.0.0 (decide explicitly):
- 687 -> 2.1.0 (ADR-142 Proposed). 409 -> 2.3.0/3.0.0. 641 post-2.0.0. 892/891/891.4 -> 2.1.0 unless wanted. 486/484/938-base -> otgw-1.x.x. 708 chore anytime.

EXEC SEQUENCE: (1) answer WS-B -> drain WS-A via loop, each build+eval+bump+push origin/dev. (2) interactive device: C-4+C-3+C-1. (3) unattended: C-2 heap soak -> Phase3 call. (4) flip B-6 -> EPIC closes -> cut 2.0.0 beta.
BOTTOM LINE: ~1 large code task (935-sat) + handful small. Rest is press-button-on-hardware + 6 decisions.
<!-- SECTION:PLAN:END -->
