---
id: TASK-837
title: >-
  Crash-proof heap fragmentation: maxBlock gate + free-only emergency recovery +
  MQTT reconnect back-off
status: To Do
assignee: []
created_date: '2026-06-07 08:35'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
George's Wemos D1 crash-loops on firmware >=1.6.0 (clean on 1.2.0-1.5.0). 6-version field-bisect proves a heap-FRAGMENTATION regression: largest-contiguous-block collapses from >3KB to <400B within 60s, MQTT_drops climb ~75/min, emergencyHeapRecovery fires with delta=+0 then the device crashes (Exception, leading hypothesis StoreProhibited/null-store). Discovery (allocation-free), HTTP/REST String churn, and WS broadcast are ruled out as the fragmenter; leading suspect is lwIP/TCP churn from the MQTT reconnect storm. This task ships the first beta: crash-proofing + reconnect back-off + the free CORS reference-bind. Plan: ~/.claude/plans/it-s-time-to-come-humble-hopcroft.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 emergencyHeapRecovery() is free-only under HEAP_CRITICAL: OTGWstream.stop() without startOTGWstream(); listener re-armed from doBackgroundTasks() only after heap recovers to >=HEAP_LOW
- [ ] #2 startOTGWstream() early-returns (no WiFiServer::begin alloc) when maxBlock is below a safe listen-socket floor
- [ ] #3 canPublishMQTT() and canSendWebSocket() refuse with a graceful-skip + counter when ESP.getMaxFreeBlockSize() < MQTT_PUBLISH_MIN_MAXBLOCK (1536); getMaxFreeBlockSize() only called when heap tier != HEALTHY
- [ ] #4 beginMqttPublish() performs the same maxBlock pre-flight as defense-in-depth before MQTTclient.beginPublish()
- [ ] #5 Distinct counters (iMqttMaxBlockSkips/iWsMaxBlockSkips) added, separate from existing drop counters, surfaced in logHeapStats/telnet
- [ ] #6 sendCorsOriginHeader() uses a const String& reference bind instead of a String copy (zero new buffers, no behavior change)
- [ ] #7 handleMQTT() reconnect retry uses exponential back-off (was flat 3s) to reduce lwIP socket churn; publish spacing is NOT reintroduced (ADR-076)
- [ ] #8 python build.py exits 0 and python evaluate.py --quick shows no new failures
<!-- AC:END -->
