---
id: TASK-1164
title: >-
  Fix: send-buffer pre-flight gives up before one ACK round trip, dropping the
  tail of every publish burst
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-24 20:01'
updated_date: '2026-09-26 14:48'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-26 PARKED at the maintainer's request.

State:
- Fix committed locally in the commit above, NOT pushed. Build green (1.7.6-beta.7+e7696a9), evaluate --quick 36/36. OTA-flashed to the bench 192.168.88.68 on 2026-09-24 22:07.
- Measurement started 2026-09-24 22:08 (simulator + test broker on 192.168.88.32:1884). The counter loop was killed at 22:11 by a host low-memory reap, but the simulator, the test broker and the subscriber kept running for ~2 days. Partial result in the first minutes: 81 OT value updates, 0 sequence gaps, mqtt_sndbuf_skips 0 at 22:11. The two early blocks showed settings 9/15 and 6/15, but those were the connect-time publishes, not full 5-minute blocks: inconclusive.
- The subscriber log (scratchpad sub1164.log) holds ~2 days of broker traffic from this build and is the next thing to analyse (5-minute block completeness over many blocks).
- The bench went OFFLINE: the last broker message is 2026-09-26 16:27:59; no ping and no HTTP since. Cause unknown. It runs the fixed build, so a link to the fix must be ruled out before this ships: read /api/v2/device/crashlog and reboot_log.txt when it is back.
- The restore POSTs (broker back to homeassistant.local:1883) could NOT reach the device. Its setting still points at 192.168.88.32:1884, and the test broker is now stopped, so when the bench comes back it has no MQTT and Home Assistant gets no data from it until the broker setting is restored. Simulator state: stop POST also did not arrive; check /api/v2/simulate when back.

Next when resumed: 1) bring the bench back (RTS recovery if needed), 2) restore broker + stop sim, 3) crashlog/reboot_log, 4) analyse sub1164.log, 5) rerun AC #1-#3.

2026-09-26: continuation is TASK-1165 (resume task with full state, rig and next steps). Test artefacts copied to logs/task-1164-sndbuf/.
<!-- SECTION:NOTES:END -->
