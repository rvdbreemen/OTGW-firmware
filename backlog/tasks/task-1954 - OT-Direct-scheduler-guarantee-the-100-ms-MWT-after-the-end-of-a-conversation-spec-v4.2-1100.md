---
id: TASK-1954
title: >-
  OT-Direct scheduler: guarantee the 100 ms MWT after the end of a conversation
  (spec v4.2 :1100)
status: To Do
assignee: []
created_date: '2026-11-15T02:12:00Z'
labels:
  - otdirect
  - spec
dependencies: []
priority: low
ordinal: 1000000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Spin-off of the ADR-241 review (92, d3, 2026-11-15). OpenTherm spec v4.2 :1100: the master waits 100 ms (MWT) from the END of the previous conversation before it starts a new one. scheduleMasterRequest() gaps the gateway's own frames by otMinIntervalMs (MI=, default 100 ms) counted from otLastAnySendMs, the START of the previous send, and waits for !otMasterRequestActive. With a late boiler answer, send start + MI can fall less than 100 ms after the answer, so an own frame can start inside the MWT. The OpenTherm library only enforces RESPONSE_TIME (50 ms) after a received frame (OpenTherm.cpp:443). The TASK-1812 relay (ADR-241, not merged yet) already waits OTD_MWT_MS from otdLastConvEndMs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real scheduleMasterRequest() with fake time: with a boiler answer late enough that send start + MI ends < 100 ms after the answer, the next own frame starts at least 100 ms after the answer; red on the current code, green on the fix
- [ ] #2 MI= keeps its meaning as a minimum gap between sends; the MWT is an additional floor
<!-- AC:END -->
