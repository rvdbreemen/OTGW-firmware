---
id: TASK-837
title: >-
  Crash-proof heap fragmentation: maxBlock gate + free-only emergency recovery +
  MQTT reconnect back-off
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-07 08:35'
updated_date: '2026-06-07 08:51'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George's Wemos D1 crash-loops on firmware >=1.6.0 (clean on 1.2.0-1.5.0). 6-version field-bisect proves a heap-FRAGMENTATION regression: largest-contiguous-block collapses from >3KB to <400B within 60s, MQTT_drops climb ~75/min, emergencyHeapRecovery fires with delta=+0 then the device crashes (Exception, leading hypothesis StoreProhibited/null-store). Discovery (allocation-free), HTTP/REST String churn, and WS broadcast are ruled out as the fragmenter; leading suspect is lwIP/TCP churn from the MQTT reconnect storm. This task ships the first beta: crash-proofing + reconnect back-off + the free CORS reference-bind. Plan: ~/.claude/plans/it-s-time-to-come-humble-hopcroft.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 emergencyHeapRecovery() is free-only under HEAP_CRITICAL: OTGWstream.stop() without startOTGWstream(); listener re-armed from doBackgroundTasks() only after heap recovers to >=HEAP_LOW
- [x] #2 startOTGWstream() early-returns (no WiFiServer::begin alloc) when maxBlock is below a safe listen-socket floor
- [x] #3 canPublishMQTT() and canSendWebSocket() refuse with a graceful-skip + counter when ESP.getMaxFreeBlockSize() < MQTT_PUBLISH_MIN_MAXBLOCK (1536); getMaxFreeBlockSize() only called when heap tier != HEALTHY
- [x] #4 beginMqttPublish() performs the same maxBlock pre-flight as defense-in-depth before MQTTclient.beginPublish()
- [x] #5 Distinct counters (iMqttMaxBlockSkips/iWsMaxBlockSkips) added, separate from existing drop counters, surfaced in logHeapStats/telnet
- [x] #6 sendCorsOriginHeader() uses a const String& reference bind instead of a String copy (zero new buffers, no behavior change)
- [x] #7 handleMQTT() reconnect retry uses exponential back-off (was flat 3s) to reduce lwIP socket churn; publish spacing is NOT reintroduced (ADR-076)
- [x] #8 python build.py exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
First-beta crash-proofing for the 1.6.0+ heap-fragmentation crash-loop (commit c715ba8c, pushed to origin/dev via c143b8da).

WHY: 6-version field-bisect proved a fragmentation regression in the 1.6.0 cycle - largest contiguous block collapses >3KB->~300B within 60s while total free still looks ok, so the next contiguous alloc returns NULL and is written (StoreProhibited). Not total-heap exhaustion.

WHAT (defense-in-depth, root-cause-agnostic):
- emergencyHeapRecovery() free-only under CRITICAL: stops OTGWstream but no longer calls startOTGWstream() (WiFiServer::begin allocates a listen socket mid-crisis - matched the observed delta=+0-then-reboot). Re-armed by new serviceDeferredStreamRearm() from doBackgroundTasks() once heap is HEALTHY.
- startOTGWstream() early-returns below MQTT_PUBLISH_MIN_MAXBLOCK (1536).
- canPublishMQTT()/canSendWebSocket()/beginMqttPublish() add a maxBlock pre-flight -> graceful skip + new iMqttMaxBlockSkips/iWsMaxBlockSkips counters (telnet logHeapStats + MQTT stats). getMaxFreeBlockSize() only walked when tier != HEALTHY.
- handleMQTT() exponential reconnect back-off 3/6/12/24/48s (connection spacing, NOT publish spacing - ADR-076 kept intact), reset to base on connect. Also breaks the publish-fail->disconnect->reconnect churn loop via the beginMqttPublish skip.
- sendCorsOriginHeader() reference-binds the Origin header (no per-response String copy).

VERIFY: build.py --firmware exit 0; evaluate.py --quick 34 passed / 0 failed / 100%. Construction argument: every firmware publish/alloc path now maxBlock-gated + the recovery no longer allocs under CRITICAL, so the firmware-path StoreProhibited class is unreachable.

NOT YET FIXED / PENDING (why this stays In Progress): (1) field validation - George flashes the beta, 24h, expect maxBlock floor stops collapsing, fragskip counters increment instead of Exception, 0 reboots; (2) the exact lwIP/internal fragmenter is still unpinned - needs the epc1 decode from the shipped crashlog poller (addr2line vs the build .elf) to confirm whether Layer-4 reconnect back-off fully neutralizes it or only crash-proofs.
<!-- SECTION:FINAL_SUMMARY:END -->
