---
id: TASK-1029
title: >-
  docs(adr): draft ADR superseding ADR-089/121 — ESP32-S3 heap-frag gating found
  unnecessary (TASK-956 evidence)
status: Done
assignee:
  - '@claude'
created_date: '2026-07-06 20:55'
updated_date: '2026-07-06 21:02'
labels: []
dependencies: []
ordinal: 238000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-956's 10-hour representative-load heap soak (esp32-classic bench, 2026-07-06) plus the June 30 data point found zero heap-tier escalations and a comfortable max-free-block floor throughout. User accepted this as sufficient evidence and directed drafting a superseding ADR for ADR-089 (heap tier-machine contract) and ADR-121 (per-consumer heap gate, WS/MQTT ladders) before any code removal.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 New ADR authored via adr-kit, Status: Proposed, explicitly supersedes ADR-089 and ADR-121
- [x] #2 Cites both TASK-956 soak data points (June 30 partial + July 6 full 10h) as Evidence
- [x] #3 Documents that Phase-3 code removal (drip/tier gating, evaluate.py ADR-089/121 gates) is NOT done yet -- this task is ADR authoring only
- [x] #4 ADR passes adr-kit's four Proposed-to-Accepted gates before any future Accept step (not required to pass yet while Proposed)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Authored ADR-167 via the adr-generator subagent (adr-kit tooling), Status: Proposed, explicitly superseding ADR-089 (heap tier machine) and ADR-121 (per-consumer WS/MQTT gating) on the dev (ESP32-S3-only) branch. Cites both TASK-956 soak data points (2026-06-30 extreme-synthetic, 2026-07-06 10-hour representative) with exact counters, including the 10,740-byte hd_min_max_block floor held across the full 10h run. Honestly flags the evidence's limits (10h vs the originally-scoped 24-72h window, missing MQTT-discovery-republish traffic) and does not self-accept -- explicitly leaves that determination to the maintainer. Does not scope or implement Phase-3 code removal; that stays a separate future task (TASK-956 AC#4).
<!-- SECTION:FINAL_SUMMARY:END -->
