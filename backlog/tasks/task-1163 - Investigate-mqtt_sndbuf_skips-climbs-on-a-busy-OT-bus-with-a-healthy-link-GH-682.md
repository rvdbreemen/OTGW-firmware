---
id: TASK-1163
title: >-
  Investigate: mqtt_sndbuf_skips climbs on a busy OT bus with a healthy link (GH
  #682)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-24 18:36'
updated_date: '2026-09-24 18:36'
labels:
  - bug
  - mqtt
  - investigation
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 232000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On beta.5 jaronbor (GH #682) reports mqtt_sndbuf_skips 71 -> 235 over 1h43m with mqtt_desync_drops 0, WiFi 92-94%, stable heap and no disconnects after boot. The guidance given on #682 (climbing after boot = stalling link) does not fit that data and was corrected on 2026-09-24. Hypothesis: the send-buffer pre-flight defers publishes during Status bursts on a busy bus. A deferred value publish is a lost update unless something republishes it, so this may mean Home Assistant misses or lags values. The beta.5 bench runs had no bus traffic, so the baseline was never measured.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Skip rate measured on the bench gateway with simulated OT bus traffic, with the gateway publishing to a broker that is subscribed to losslessly
- [ ] #2 For each deferred publish it is established from code whether the value is lost, retried, or republished later (on-change, heartbeat, discovery drip), with file:line
- [ ] #3 Measured: whether values published by the device differ from what a lossless broker subscription received during the run (lost updates counted)
- [ ] #4 Conclusion and next step posted on GH #682, stated as measured fact or explicitly as hypothesis
<!-- AC:END -->
