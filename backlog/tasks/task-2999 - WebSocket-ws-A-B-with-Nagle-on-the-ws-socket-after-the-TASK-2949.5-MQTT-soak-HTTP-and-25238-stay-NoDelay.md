---
id: TASK-2999
title: >-
  WebSocket /ws A/B with Nagle on the ws socket (after the TASK-2949.5 MQTT
  soak; HTTP and 25238 stay NoDelay)
status: To Do
assignee: []
created_date: '2026-11-21T08:18:00Z'
labels:
  - mqtt
  - websocket
  - heap
milestone: 2.0.0
dependencies:
  - TASK-2949.5
priority: medium
ordinal: 1124000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's decision 2026-11-21 (relayed by the coordinator): 'Volg het advies. MQTT afmaken. Na soak de websocket testen met Nagle en webserver zo laten.' Order: (1) finish MQTT Nagle in TASK-2949.5 (.63, per-event incl. republish, 60 min soak, then the ADR-266 Accept to Robert). (2) AFTER that soak, this task: an A/B of Nagle on the /ws socket only: client()->setNoDelay(false) at the ws connect (webBeginRequest/onEvent), no library change. (3) HTTP port 80 and port 25238 stay NoDelay; telnet 23 already has Nagle. Basis: the architect's ADR-267 draft. Method as TASK-2949.5 on the NAS: n=3 boots per arm alternating, arm identified by sha256, bench by otgw-bench-tester.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A/B build pair: arm ws-nagle (setNoDelay(false) on the /ws client at connect) vs control, one source difference, proven by objdump and different sha256
- [ ] #2 n=3 boots per arm on a bench unit (otgw-bench-tester, NAS method): hd_min_free_heap, largest free block, web_sndq_hwm, largest drop inside one loop() with a /ws client connected
- [ ] #3 Live-update latency per arm: /ws frame timestamp vs the same value on MQTT/telnet, p50/p95, with build githashes and config per run
- [ ] #4 Decision to Robert by DM (via the coordinator) with the numbers; if Nagle on /ws is chosen, ADR-267 updated and the setting placed in one spot with a comment citing the ADR and numbers
<!-- AC:END -->
