---
id: TASK-1094
title: Reconcile the 1.x DHW water total with the 2.0.0 peer decision
status: Done
assignee:
  - '@claude'
created_date: '2026-08-27 04:26'
updated_date: '2026-09-04 06:34'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 193000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-090 Must #2 mandates the same accumulation method on the 1.x and 2.0.0 lines so both firmwares report the same total for the same boiler. The 1.x implementation (TASK-1091, TASK-1093) was built without reading the 2.0.0 peer record, which lives in the other worktree. ADR-094 carries this as a known open point.

Read the peer record and compare the two implementations on the points that change the number a user sees: whether the peer persists the counter, what its gap rule is and what cap value it uses, and whether it applies each sample to the preceding or the following interval. Then either align the two or record, on both lines, why they deliberately differ.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 2.0.0 peer record is identified by number and its decision on persistence, gap rule and cap value is summarised against the 1.x implementation
- [x] #2 Any divergence that changes the reported total is either fixed on one line or recorded as deliberate in a decision record on both lines
- [x] #3 ADR-094's open question on 2.0.0 parity is answered with a pointer to the outcome
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-04: peer record read and compared. The peer is ADR-176 on the 2.0.0 line (dev worktree), status Accepted.

The finding that reframes this task: the 2.0.0 water total is NOT IMPLEMENTED. No file under src/ on dev references dhw_water_total, WaterTotal or waterTotal. There is a decision but no code, so there is no divergent implementation to reconcile, only a divergent plan.

Comparison on the points that change the number a user sees:

- Entity contract: IDENTICAL. Both are device_class water, unit L, state_class total_increasing, published only once a MsgID 19 frame has decoded. ADR-176 binds exactly this ("Keep the entity contract (name, unit, device class, state class) identical to the 1.x line's peer decision").
- Accumulation method: IDENTICAL. Both use elapsed time times flow with a zero-hold rule and a capped usable interval, biasing to undercount. ADR-090 Must #2 is satisfied. 1.x: dhwWaterMeter.ino:67, dhwWaterTotalL += flow * (dtMs / 60000.0f).
- Cap value: NO DIVERGENCE YET. 1.x uses 60000 ms (dhwWaterMeter.ino:38, DHW_METER_MAX_GAP_MS), matching the firmware's own 60 s publish cadence. ADR-176 states the rule ("the usable interval capped so a long sampling gap cannot invent litres") without ever naming a number, so 2.0.0 has not yet chosen one.
- Persistence: DELIBERATE DIVERGENCE, ALREADY RECORDED ON BOTH LINES. 2.0.0 persists to its own file (write when delta >= 10 L, or 15 min elapsed and delta > 0, plus one write on graceful reboot). 1.x keeps the counter in RAM and loses it on reboot. This needs no new decision record, because ADR-176's own Decision Contract already unbinds it: "Storage mechanism is explicitly not bound by that mandate." The 1.x side is recorded in ADR-093 and ADR-094.
- Reset surface: follows from persistence. 2.0.0 mandates a paired REST and MQTT reset; 1.x deliberately has none, because a reboot zeroes the counter (ADR-094 open question, answered).

One stale premise found, which cannot be fixed here: ADR-176's answered question on unclean-reboot regression states "Same answer as the 1.x peer". That was true on 2026-08-24 under ADR-090. ADR-093 then superseded ADR-090 and removed persistence from 1.x, so the assumed symmetry no longer holds. ADR-176 is Accepted and therefore immutable, so it must not be edited. Recorded instead as TASK-1123 on the 2.0.0 line (commit 7e5fa5595, pushed to origin/dev), which also carries the 60000 ms cap that line must adopt.

No code change on either line. The reconciliation is that the two lines already agree everywhere ADR-090 Must #2 binds them, and the one real difference was pre-authorised by the peer record itself.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Reconciled: the two lines already agree everywhere the parity mandate binds them, and the single real difference was pre-authorised by the peer record. No code change on either line.

The peer is ADR-176 (2.0.0, Accepted). It is a decision with no implementation behind it: nothing under src/ on dev references the water total, so the 1.x build shipped in v1.7.5 is now the reference rather than one of two parallel plans.

Identical: the entity contract (device_class water, unit L, state_class total_increasing, announced only after a MsgID 19 frame decodes) and the accumulation method (elapsed time times flow, zero-hold, capped interval, biasing to undercount). That is what ADR-090 Must #2 actually requires.

Different, and deliberately so: 2.0.0 plans to persist the counter with a bounded flash-write rule and a paired reset surface; 1.x keeps it in RAM with no reset, because a reboot zeroes it. No new decision record was needed, because ADR-176 already states that the storage mechanism is not bound by the parity mandate, and ADR-093 and ADR-094 record the 1.x side.

Not yet divergent, but at risk: ADR-176 states the interval-cap rule without a number, while 1.x uses 60000 ms. A different number on 2.0.0 would make the two firmwares report different totals for the same boiler.

Carried to the peer line as TASK-1123 (commit 7e5fa5595 on origin/dev): adopt the 60000 ms cap, and note that ADR-176's "same answer as the 1.x peer" on unclean-reboot regression was written before ADR-093 removed persistence from 1.x. ADR-176 is Accepted and immutable, so the correction lives in the task rather than in the record.
<!-- SECTION:FINAL_SUMMARY:END -->
