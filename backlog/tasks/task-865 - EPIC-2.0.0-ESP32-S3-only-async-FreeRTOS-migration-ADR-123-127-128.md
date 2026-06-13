---
id: TASK-865
title: 'EPIC: 2.0.0 ESP32-S3-only async + FreeRTOS migration (ADR-123/127/128)'
status: To Do
assignee: []
created_date: '2026-06-13 05:37'
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
