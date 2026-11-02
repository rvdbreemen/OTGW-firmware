---
id: TASK-1536
title: >-
  SAT: every transmitted CS is capped by the effective max setpoint
  (thermal-safe fallback, 5 s resend, PIC keepalive)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-01T00:01:00Z'
updated_date: '2026-11-02T04:50:00Z'
labels:
  - sat
  - low-load
dependencies: []
priority: high
ordinal: 692000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.2 second read (2026-11-01): the thermal-safe fallback caps CS with satGetMaxSetpoint() (user MaxTSet only, SATcontrol.ino:6515-6522) instead of satEffectiveMaxSetpoint() (:405, which also includes the boiler's own MaxTSet), and the 5 s resend (:634-641) and the PIC CS keepalive (:597-606) repeat the last CS without clamping it again after a limit change. Related: TASK-1412.13 (MaxTSet negotiation and every transmitted ceiling), entries 48/51/54/67/100/110/148/152.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test on the real code: thermal-safe fallback, resend and keepalive never send a CS above satEffectiveMaxSetpoint(), also right after the limit is lowered (red on HEAD, green)
- [x] #2 Decision check against entries 148/152 recorded in the notes before the change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) satCapRepeatCS(): the 5 s resend and the PIC keepalive cap the repeated CS at satEffectiveMaxSetpoint() and store the capped value in the cache. 2) satThermalSafeSetpoint() = min(SAT_THERMAL_SAFE_FLOW, satEffectiveMaxSetpoint()) replaces the user-MaxTSet-only cap in the thermal-safe branch. 3) tests/test_sat_cs_ceiling_1536.py on the real code, red on HEAD; stub satEffectiveMaxSetpoint() in the 4 harnesses that compile the command block.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: thermal-safe CS=45 capped by satGetMaxSetpoint (user MaxTSet) not satEffectiveMaxSetpoint (boiler MaxTSet), SATcontrol.ino:6549; resend :663 and keepalive :629 repeat last CS unclamped. Agent-doable now.

AC#2 decision check (2026-11-02): entry 148 'SAT still caps every CS at the default (62 radiators, 50 underfloor)' and entry 152 'that value ... is the only cap: SH= carries it and SAT caps CS at it'. Both say EVERY CS is capped; the repeats and the fallback were the exceptions, so this change applies the decided rule to all senders and decides nothing new. thermo-nova (pinned 7beaaf5, opened this session): the CS clamp is clamp(setpoint, MINIMUM_SETPOINT, coordinator.maximum_setpoint) (climate.py:379, :394), and maximum_setpoint is the user or default limit (coordinator/__init__.py:275-283); the boiler's own MaxTSet is only exposed (coordinator/mqtt/opentherm.py:79-83), not used in that clamp. The boiler-MaxTSet term is a firmware rule that the control tick already applies (satEffectiveMaxSetpoint, TASK-1373); this task extends it to the repeat paths and the firmware-only thermal-safe fallback (no thermo-nova equivalent). No new behaviour on the tick path, so no Sergeant D question; mentioned in the overview as a notice.
<!-- SECTION:NOTES:END -->
