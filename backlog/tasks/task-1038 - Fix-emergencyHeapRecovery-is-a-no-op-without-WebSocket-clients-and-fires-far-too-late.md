---
id: TASK-1038
title: >-
  Fix: emergencyHeapRecovery is a no-op without WebSocket clients and fires far
  too late
status: To Do
assignee:
  - '@claude'
created_date: '2026-07-19 14:59'
updated_date: '2026-08-25 20:46'
labels: []
dependencies: []
priority: high
ordinal: 157000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evidence from martreides 1.7.1 captures (otgw-171.log, otgw-171-2.log, TASK-1037): every emergencyHeapRecovery() invocation logs 'delta=+0 actions=0x06'. Bit 0x01 (drop WebSocket clients) is never set, so no browser was connected. Of the three ADR-079 actions in helperStuff.ino:1171, action 1 does nothing without WS clients, action 2 (OTGWstream.stop()) does nothing without stream clients, and action 3 (clearMQTTConfigPending()) clears a static bitmap and returns no bytes. Recovery is therefore structurally incapable of reclaiming anything in the most common field configuration: MQTT-only, no browser open.

Second defect: the trigger is too late. OTGW-firmware.ino:403 only calls it at HEAP_CRITICAL, which is freeHeap < 1536 (helperStuff.ino:879). In both captures recovery first ran at before=888 bytes free, with maxBlock already around 500. At that point there is nothing left to work with and the crash follows within seconds.

Scope: give recovery at least one action that reclaims memory when no WS/stream clients exist, and move the trigger earlier (HEAP_WARNING or HEAP_LOW) so it acts while recovery is still possible. Consider whether the 30s EMERGENCY_RECOVERY_INTERVAL_MS rate limit is appropriate when the terminal collapse takes under 60s.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 emergencyHeapRecovery reclaims a measurable, logged number of bytes in an MQTT-only configuration with no WS or stream clients
- [ ] #2 Recovery trigger fires early enough that free heap is above HEAP_CRITICAL when the first attempt runs
- [ ] #3 Rate-limit interval justified against the observed collapse rate, with the reasoning recorded in the task
- [ ] #4 delta=+0 no longer appears in a reproduction of the martreides collapse profile
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Strict ordering. Step 2 is worthless before step 1 lands: firing a no-op earlier still reclaims nothing. Zero bytes at 03:02 is as useless as zero bytes at 03:04.

1. Make recovery actually reclaim in the MQTT-only, no-browser configuration.
   - Audit what is actually holding heap in the martreides collapse profile (lwIP pcbs and rx buffers of unserviced HTTP connections are the prime suspect, see TASK-1039).
   - Give emergencyHeapRecovery at least one action that returns bytes when hasWebSocketClients() is false and no OTGWstream clients exist.
   - Verify by log: delta must be measurably positive in a reproduction. This is the gate on the whole task.

2. Only once step 1 shows delta > 0, move the trigger earlier.
   - OTGW-firmware.ino:403 currently fires only at HEAP_CRITICAL (freeHeap < 1536). Both captures show the first attempt at before=888, far past the point of rescue.
   - Move to HEAP_WARNING (3072) or HEAP_LOW (5120). Pick based on measured recovery yield at each level, not on intuition.
   - Watch for thrash against the existing TASK-553 drip-mode hysteresis, which already uses HEAP_LOW as its entry trigger and HEAP_LOW_RESTORE_THRESHOLD (6144) to restore.

3. Re-evaluate EMERGENCY_RECOVERY_INTERVAL_MS (30000) against the observed collapse rate.
   - In otgw-171.log the run from first gate trip to crash is under 90 seconds, so a 30s rate limit allows at most two or three attempts. Record the reasoning either way.

Do NOT raise HTTP_SERVE_MIN_MAXBLOCK or MQTT_PUBLISH_MIN_MAXBLOCK as part of this task. Those gates throttle consumers rather than reclaiming memory, they are field-calibrated against the 1460-byte TCP MSS cliff, and raising them makes the TASK-1039 latch engage earlier.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-25: parked, needs a superseding ADR before any code. ADR-079 is Accepted and its Decision binds exactly what this task wants to change: the trigger is fixed at getHeapHealth() == HEAP_CRITICAL, the 30-second EMERGENCY_RECOVERY_INTERVAL_MS is named in the same sentence, and the action list is exactly three items. Moving the trigger to WARNING or LOW contradicts it, and adding a fourth action contradicts the closed list.

Worse for this task specifically: ADR-079 already considered and REJECTED the obvious fourth action. Its 'Explicitly NOT done' section rules out dropping telnet (the operator needs the diagnostic window during an incident) and rules out an MQTT disconnect/reconnect on the grounds that the reconnect cost, 15 seconds blocking plus a retained-state resync burst, exceeds the heap it recovers. Any new action has to beat that reasoning, not ignore it.

So the work is: draft a superseding ADR that (a) argues the tier change from evidence, since the martreides captures show first recovery at 888 bytes free with maxBlock already around 500, which is past the point where anything can be reclaimed, and (b) names an action that reclaims memory in an MQTT-only, no-browser configuration without falling into what ADR-079 rejected. Then grill it, then have the maintainer accept it, then implement.

Related and already fixed: TASK-1039 removed the HTTP gate latch, so one source of unreleased sockets during the same window is gone. That does not close this task, because recovery still has no action with any effect when no WebSocket or stream client exists.

2026-08-25: closed as wontfix. The premise does not survive contact with TASK-1037's own findings.

This task was built on 'every emergencyHeapRecovery() invocation logs delta=+0 actions=0x06', read as evidence that recovery is structurally incapable of reclaiming anything. TASK-1037 records the same observation and draws the opposite, correct conclusion: 'free heap and maxBlock fall together (frag stays ~3%), and emergencyHeapRecovery reports before=888 after=888 delta=+0 actions=0x06 - recovery reclaims zero bytes, meaning everything allocated is still referenced.'

delta=+0 was therefore not a defect in recovery. It is the signature of a leak. Recovery reclaimed nothing because nothing was reclaimable: every allocation was still referenced. The helper behaved correctly.

TASK-1037 is Done. The leak it identified (DHCP option 42 pushing SNTP servers on every lease renewal, plus the mDNS null-allocation crash on an exhausted heap) was fixed and shipped in v1.7.2. The captures this task reasons from are 1.7.1, i.e. from before that fix.

The other premise also does not hold. Baseline headroom on this firmware is not scarce: the same captures show heap flat at around 20 KB for 35 to 40 minutes after boot, and v1.7.0 had already reclaimed roughly 6.6 KB of static RAM and restored the under-load contiguous-block floor to about 11 KB. The 888-byte state this task treats as the operating point is the terminal stage of a leak, not the normal condition. Designing an earlier trigger and a fourth recovery action around it would be optimising for a state the firmware no longer reaches.

Not closed as 'already fixed', because nothing here was fixed: the correct reading is that there was never a defect in emergencyHeapRecovery to fix. ADR-079 stands as written, and no superseding ADR is needed.

If HEAP_CRITICAL is ever observed again on a post-1.7.2 build, that warrants a fresh task with fresh captures rather than reviving this one. The distinguishing question for such a report: do free heap and maxBlock fall together (a leak, as here) or does maxBlock collapse while free heap holds (fragmentation, which is what the 1.7.0 gates address)?
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as wontfix. No code changed and none was needed.

The task read 'delta=+0' from emergencyHeapRecovery as proof that recovery cannot reclaim anything in an MQTT-only configuration, and proposed a fourth recovery action plus an earlier trigger tier. TASK-1037 recorded the same log line and identified what it actually means: everything allocated was still referenced, because the device was leaking. Recovery reclaiming zero bytes was the correct outcome, not a malfunction.

That leak (DHCP option 42 SNTP injection per lease renewal, and the mDNS null-allocation crash under exhaustion) was fixed and shipped in v1.7.2 under TASK-1037, which is Done. This task's evidence is from 1.7.1 captures that predate the fix.

The supporting premise fails too: the firmware is not memory-starved. The same captures show about 20 KB free for the first 35 to 40 minutes, and v1.7.0 had already reclaimed roughly 6.6 KB of static RAM. The 888-byte reading is the end state of a leak, not the operating point.

Implementing this would have required a superseding ADR against ADR-079, which explicitly fixes the CRITICAL trigger, the 30-second interval and the three-action list, and which had already considered and rejected the obvious additions (dropping telnet, reconnecting MQTT) with reasons that still hold. Writing that ADR to optimise for a state the firmware no longer reaches would have made the record worse, not better.

ADR-079 stands unchanged. A future HEAP_CRITICAL report on a post-1.7.2 build should open a new task with new captures; the question that separates the two failure modes is whether free heap and maxBlock fall together (leak) or maxBlock collapses alone (fragmentation).
<!-- SECTION:FINAL_SUMMARY:END -->
