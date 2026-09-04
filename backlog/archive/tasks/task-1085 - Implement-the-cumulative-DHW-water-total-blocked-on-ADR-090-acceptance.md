---
id: TASK-1085
title: Implement the cumulative DHW water total (blocked on ADR-090 acceptance)
status: To Do
assignee: []
created_date: '2026-08-24 20:22'
updated_date: '2026-09-04 05:59'
labels:
  - enhancement
dependencies: []
priority: low
ordinal: 186000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-on from TASK-1081 and GitHub #675. The rate sensor is now correctly typed as volume_flow_rate, but the Energy dashboard needs a cumulative volume entity that does not exist yet. ADR-090 proposes it and answers the design questions (time x flow zero-hold with a capped interval, resume-and-undercount on unclean reboot, own file not settings.ini, write on delta >= 10 L or a 15-minute floor, paired REST+MQTT reset). BLOCKED: ADR-090 is still Proposed and needs maintainer acceptance before implementation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-090 is Accepted before any code is written
- [ ] #2 Cumulative total published as device_class water, unit L, state_class total_increasing, and selectable as an Energy dashboard water source on a live HA instance
- [ ] #3 Accumulation is elapsed-time x flow, hooked at both state write sites, and one real litre of flow is counted exactly once (bench-verified, including OTDirect master mode with a thermostat on the 2.0.0 peer)
- [ ] #4 Counter persists across reboot, never decreases, and is resettable through the paired REST and MQTT surface
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-08-25: unblocked. ADR-090 accepted by the maintainer. Its peer ADR-176 is accepted on the 2.0.0 line, so the entity contract is agreed across both lines before either implementation starts.

Design is settled and does not need re-litigating: accumulate as elapsed time times flow (never a fixed volume per sample), zero-hold with a capped interval biased to under-count, resume-and-accept-undercount on unclean reboot so the counter never decreases, persist in its own small file rather than settings.ini, write on delta >= 10 L or a 15-minute floor plus one write on graceful reboot, and expose a paired REST and MQTT reset.

Two traps recorded in the ADR that are easy to miss when implementing: if the entity gets a faux message id it must be registered in the boot-publish path for non-OT discovery configs or it is simply absent in HA until the first value arrives; and on the 2.0.0 peer the accumulator must be called from BOTH state write sites, because updatePSSummaryFloatState bypasses print_f88 entirely.

2026-09-04 board cleanup: superseded, not implemented as written.

The cumulative DHW water total shipped in v1.7.5 under TASK-1091, and the design it shipped with is deliberately NOT the one this task specifies. ADR-090 (which this task was blocked on) is Superseded by ADR-093, which is in turn Superseded by ADR-094.

Two of the four ACs here are now wrong rather than merely undone:
- AC #3 requires accumulation hooked at both state write sites with bench verification including OTDirect master mode on the 2.0.0 peer. The shipped rule instead treats an interval longer than 60 seconds between MsgID 19 frames as a gap in the measurement, because a bus that falls silent at 8 l/min must not keep booking water that never flowed.
- AC #4 requires the counter to persist across reboot. ADR-093 and ADR-094 decided the opposite and keep it in RAM: state_class total_increasing reads the post-reboot drop as a meter reset and preserves the long-run sum, while an unclean-power-loss restore would corrupt the Home Assistant statistic. Implementing AC #4 would undo a shipped, released decision.

Working this task as specified would regress v1.7.5. Archiving rather than marking Done, because the work it describes was never performed and should not be.

Remaining live question, the 1.x versus 2.0.0 parity that ADR-094 left open, is carried by TASK-1094.
<!-- SECTION:NOTES:END -->
