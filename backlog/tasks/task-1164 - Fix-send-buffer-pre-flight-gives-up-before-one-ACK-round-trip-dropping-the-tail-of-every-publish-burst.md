---
id: TASK-1164
title: >-
  Fix: send-buffer pre-flight gives up before one ACK round trip, dropping the
  tail of every publish burst
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-24 20:01'
updated_date: '2026-09-24 20:02'
labels:
  - bug
  - mqtt
dependencies:
  - TASK-1163
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/682'
priority: high
ordinal: 233000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1163 measured that mqttFrameFitsSndbuf() (MQTTstuff.ino:377-393) waits only 10 yield() calls for tcp_sndbuf room, which is shorter than one ACK round trip to the broker. The ~30-publish burst of do5minevent() therefore loses its tail (otgw-pic/settings/*) every 5 minutes, and the hourly heapdiag burst its tail as well; non-OT publishes have no retry. Regression from TASK-1154, reported on GH #682 (jaronbor ~1.6 skips/min on a healthy link).

Fix: wait for room up to a wall-clock budget instead of a yield count, and after a deferral skip the wait for a short back-off so a broker that has stopped reading costs one wait per burst, not one per publish.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On the bench with the TASK-1163 setup (simulated bus, lossless broker subscription), every topic of the 5-minute burst including otgw-pic/settings/* reaches the broker in each block, and mqtt_sndbuf_skips stays flat
- [ ] #2 OT value updates still arrive complete (lossless comparison, as in TASK-1163)
- [ ] #3 Against a broker that stops reading, the gateway stays responsive: the longest REST gap stays bounded (no worse than the 4.9 s measured for TASK-1155) and mqtt_desync_drops stays 0
- [ ] #4 Build (build.bat, fresh bins, success line) and evaluate.py --quick green
<!-- AC:END -->
