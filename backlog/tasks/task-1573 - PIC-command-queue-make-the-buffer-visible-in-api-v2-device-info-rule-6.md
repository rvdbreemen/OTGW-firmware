---
id: TASK-1573
title: 'PIC command queue: make the buffer visible in /api/v2/device/info (rule 6)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-02T21:11:00Z'
updated_date: '2026-11-04T10:14:00Z'
labels:
  - pic
  - command-queue
  - rest-api
milestone: 2.0.0
dependencies:
  - TASK-1569
priority: medium
ordinal: 721000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-02, queue design rule 6. Expose the PIC command queue state so drips, waits and drops can be seen without telnet: depth (cmdQueueSize), the oldest waiting command and its age, the command in flight (TASK-1571) and the PM=/TP= slot holder (rule 3), and counters: sent, retried, rejected (PIC error code), dropped (retry exceeded; today state.pic.iCmdDropped). Hand-rolled JSON (ADR-146), read under the queue lock with a bounded timeout from async_tcp (TASK-1569, ADR-like OTStateLock rule TASK-879).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 /api/v2/device/info carries a cmdqueue object: depth, oldest_cmd, oldest_age_ms, inflight, prio_slot, sent, retried, rejected, dropped (host test on the real JSON writer, or a REST test on the bench)
- [x] #2 Values move as expected on the bench: a burst of 5 REST commands raises depth and sent, an unknown command raises rejected; docs/api updated
- [x] #3 Read under the queue lock with a bounded timeout; no ArduinoJson (evaluate gate)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-02 handover to the lead (lane-q2 ../wt-q2, no bump, series 537fd5a63..HEAD, one commit per task). Host: tests/test_pic_pm_chain.py 28/28 at the top; each task's new tests red on the code before it (see commit/test names), green at its own commit; evaluate --quick 0 failed. Open: bench on .63 after the 08:00 release (lead tags, builds, benches each task), firmware compile check of the top running at handover.

2026-11-04 bench .63 (simulation off): device/info carries the queue state as flat cmdq_* fields (cmdq_valid, depth, oldest, oldest_age_ms, inflight, prio_slot, sent, retried, rejected, echo_*) plus cmd_dropped, not a nested cmdqueue object as AC1 words it. Burst of 5 REST commands: depth 1 right after; sent 0->3 after the run; unknown ZZ=1: cmdq_rejected 0->1 and cmd_dropped 0->1. With satsimulation ON nothing moves (commands consumed without sending, TASK-1580).

2026-11-04: /api/v2/device/info on .63 carries cmdq_valid, cmdq_depth, cmdq_oldest, cmdq_oldest_age_ms, cmdq_inflight, cmdq_prio_slot, cmdq_sent, cmdq_retried, cmdq_rejected, cmdq_prio_timeouts (+ echo stats) and cmd_dropped (TASK-1439). Deviation, decided by the lead: flat cmdq_* fields instead of a nested cmdqueue object, because every other device/info field is flat and the HA/REST consumers read flat keys. AC3: cmdQueueSnapshot (OTGW-Core.ino:4322) takes CmdQueueLock with CMD_QUEUE_LOCK_MS 100 and returns cmdq_valid=false when busy; evaluate --quick ArduinoJson gate 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
device/info shows the PIC queue as flat cmdq_* fields (depth, oldest, age, inflight, prio slot, sent, retried, rejected, timeouts, echo stats; dropped = cmd_dropped), read under a 100 ms bounded lock with cmdq_valid=false on contention. Evidence: live device/info on .63, code at OTGW-Core.ino:4322, evaluate 0 failed. Deviation: flat fields, not a nested object.
<!-- SECTION:FINAL_SUMMARY:END -->
