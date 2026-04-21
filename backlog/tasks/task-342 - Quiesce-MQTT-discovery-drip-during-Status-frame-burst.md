---
id: TASK-342
title: Quiesce MQTT discovery drip during Status-frame burst
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-19 21:06'
updated_date: '2026-04-19 21:13'
labels:
  - mqtt
  - heap
  - discovery
  - 1.4.1
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Add a transient flag that suspends the discovery drip while a Status-frame sub-topic fanout is in progress. Prevents the heap allocation peaks of drip and Status-burst from overlapping.

**Background**
Tester log shows consistent pattern: Status-frame (msgid 0) arrives, publishes 9 sub-topics within ~20ms, then processing continues. Meanwhile the 1s drip timer can fire mid-burst and add a discovery publish on top — that combination hits the throttle-gate.

Quiescing the drip during a Status-burst keeps the two allocation peaks from stacking.

**Design**
- Add static flag `statusBurstActive` in MQTTstuff.ino (or as state.mqtt.bStatusBurstActive via ADR-051)
- Set TRUE in processOT() just before the Status-frame sub-topic loop starts (OTGW-Core.ino)
- Clear FALSE after the last sub-topic publish completes
- loopMQTTDiscovery() checks the flag and skips publishing when set (timer keeps running, catches up next tick)
- Timeout safety: force-clear the flag if it stays set >500ms (defends against a publish failing mid-burst)

**Considerations**
- Pro: targeted fix — addresses the exact sequence visible in tester log lines 430-455
- Pro: zero impact when Status-frames are not active (idle system)
- Pro: no user-visible change
- Con: adds one cross-module flag. Acceptable size.
- Con: Status-bursts happen ~every 3s, so up to 20-30% of drip ticks may skip during boot discovery phase. Marginal slowdown acceptable.
- Alternative: use a post-Status delay in the drip timer — less precise, might still collide with other msgid fanouts.

**Impact estimate**
Removes the specific heap-collision visible at lines 450-455 of debug_2a.txt (heap=7832 low-point). Expected 20-30% additional reduction on top of options 1+2.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Flag added (location: state.mqtt.bStatusBurstActive OR static in MQTTstuff.ino — pick based on ADR-051 review)
- [ ] #2 Flag set TRUE at start of Status-frame sub-topic loop in processOT
- [ ] #3 Flag cleared FALSE after final sub-topic publish
- [ ] #4 loopMQTTDiscovery() skips publish when flag is TRUE
- [ ] #5 Timeout safety: flag auto-clears after 500ms
- [ ] #6 Build passes for esp8266 environment
- [ ] #7 Manual test: confirm drip does not fire during Status-burst by comparing timestamps in debug log
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the Status-frame (msgid 0) sub-topic fanout in src/OTGW-firmware/OTGW-Core.ino processOT() — there are both Master and Slave status Read-Ack paths that trigger 9 publishes
2. Add a file-scope static flag statusBurstActive in MQTTstuff.ino plus a pair of helpers beginStatusBurst() / endStatusBurst()
3. Timeout safety: beginStatusBurst() records millis(); endStatusBurst() clears it; loopMQTTDiscovery force-clears if burst older than 500ms
4. Wrap the Status-frame sub-topic fanout in OTGW-Core.ino with begin/end
5. loopMQTTDiscovery checks statusBurstActive and skips publishing when set (timer keeps running, next tick picks up)
6. Build esp8266
7. Commit
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Added statusBurstActive flag + beginStatusBurst/endStatusBurst/isStatusBurstActive in MQTTstuff.ino with 500ms self-heal timeout. Wrapped publishMasterStatusState and publishSlaveStatusState in OTGW-Core.ino — this catches ALL three call sites (the combined caller at line 3388, plus the individual-side callers at lines 1871 and 1899). loopMQTTDiscovery now skips a tick when isStatusBurstActive() returns true. Forward declarations in OTGW-firmware.h.
<!-- SECTION:NOTES:END -->
