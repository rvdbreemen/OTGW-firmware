---
id: TASK-1164
title: >-
  Fix: send-buffer pre-flight gives up before one ACK round trip, dropping the
  tail of every publish burst
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-24 20:01'
updated_date: '2026-09-28 19:47'
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
- [ ] #1 Build (build.bat, fresh bins, success line) and evaluate.py --quick green
- [ ] #2 Every otgw-pic/settings/* topic of the 5-minute burst reaches the broker: 15/15 in each of 3 housekeeping bursts, in two 8-minute bench runs (scratchpad brk_syncT_clean, brk_syncF_clean)
- [ ] #3 No publish arrives damaged: 0 corrupt, malformed, foreign-topic or invalid-JSON publishes in ~5400 across the TASK-1166/1168 bench runs
- [ ] #4 Against a broker that stops reading in the middle of a large payload, a stall ends within ~10 s in a clean reconnect without corrupt frames (measured longest REST gap 8.3 s; TASK-1166 A/B 0 of 8 truncated frames carry E0 00)
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

2026-09-28 bench evidence from the TASK-1166/1168 runs (.88.68, beta.7 code, scratchpad brk_syncT_*/ram_syncT_*):
- AC#1 settings part: every otgw-pic/settings/* topic arrived, 15/15 in each of 3 housekeeping bursts, in two clean 8-minute runs (setSync true and false). mqtt_sndbuf_skips flatness was not recorded in those runs.
- AC#2: not re-measured as a lossless comparison; integrity only: 0 corrupt/malformed/foreign-topic/invalid-JSON publishes in ~5 400.
- AC#3 NOT met under a harsher stall: a broker that closes its receive window 5.5-9 s in the middle of a large discovery payload gives a longest REST gap of 8.3 s (setSync false 8.0 s), bound is 4.9 s, and mqtt_desync_drops rises (2-8 per 10-15 min; since TASK-1166 each ends in a clean reconnect, never a corrupt frame). The 4.9 s of TASK-1155 was measured with a broker that stops reading between publishes.
- AC#4: build.bat and evaluate.py green for v1.7.6-beta.7.
Shipped in v1.7.6-beta.7 on the maintainer's decision. Left In Progress: AC#3 needs a decision (accept the mid-payload case or bound the payload write too).
<!-- SECTION:NOTES:END -->
