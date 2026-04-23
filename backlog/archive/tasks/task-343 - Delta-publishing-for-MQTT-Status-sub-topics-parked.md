---
id: TASK-343
title: Delta publishing for MQTT Status sub-topics (parked)
status: To Do
assignee: []
created_date: '2026-04-19 21:07'
labels:
  - mqtt
  - heap
  - parked
  - 2.0.0
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Track the last-published value for each Status sub-topic and only re-publish topics whose value actually changed since the previous Status-frame. In steady state (idle boiler) most sub-topics remain identical frame after frame.

**Background**
Status-frame fans out 9-10 sub-topic publishes every ~3s. In idle state (no heating call, no DHW, no fault) all values stay OFF and identical. Publishing the same "OFF" every 3s wastes MQTT traffic, heap, and HA broker load.

A simple last-value cache with change detection would reduce the fanout to only the topics that actually flipped.

**Considerations**
- Pro: dramatic reduction in steady-state MQTT traffic
- Pro: heap savings during Status-burst since fewer PubSubClient writes
- Pro: no breaking change — topics stay identical, just less frequent
- Con: RAM cost — need ~20 bytes of last-value cache per Status sub-topic (~200 bytes total)
- Con: HA retained messages may become stale on broker — must keep a periodic force-republish (every 60s?) for HA entity availability
- Con: first implementation risk — easy to miss edge cases (flag bits, status word changes)

**Blocker: complexity**
This is a real engineering change, not a tweak. Requires test cases covering:
- Change-detect on each bit of Status word
- Boot: first-publish must always go out
- Reconnect: full republish on MQTT reconnect (birth)
- Timer: periodic force-publish to keep HA availability fresh

**Status: Parked for 1.4.x**
Log for future consideration. If heap pressure stays an issue after options 1+2+3+5, revisit this. Otherwise parks until 2.0.0 refactor.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Not targeted for 1.4.x — implement only after 1+2+3+5 ship and tester feedback is in
- [ ] #2 If implemented: last-value cache in state.mqtt, force-republish timer, unit tests for boot/reconnect/timer paths
<!-- AC:END -->
