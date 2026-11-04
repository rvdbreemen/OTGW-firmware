---
id: TASK-1138
title: >-
  Fix: OTDirect loopback can raise bBoilerState from synthetic frames on a combo
  board
status: Done
assignee:
  - '@claude'
created_date: '2026-09-18 05:23'
updated_date: '2026-09-20 08:24'
labels:
  - bug
dependencies: []
priority: low
ordinal: 282000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found while porting TASK-1135 and deliberately left out of that change: pre-existing, and fixing it there would have mixed two concerns.

OTDirect loopback bridges synthetic B-frames into the parser. On a combo board with HAS_PIC true, those frames write the boiler last-seen stamp like any other, so state.otBus.bBoilerState can rise from simulated traffic rather than from a real boiler.

otDirectBoilerPresent() already excludes loopback explicitly. The bBoilerState branch does not, which is the inconsistency.

TASK-1135/1137 did not introduce this and does not widen it. What that change does do is make the flag fall again once the synthetic traffic stops, where before it would have stayed raised until reboot. So the window is now bounded, which arguably makes this easier to observe than it was.

Worth deciding rather than assuming: should loopback traffic count as boiler presence at all? A bench rig may want it to, so that the rest of the stack behaves as if a boiler were there. If so this is documentation, not a defect. If not, the bBoilerState branch needs the same loopback exclusion otDirectBoilerPresent() already has.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A decision is recorded on whether loopback traffic should count as boiler presence, with the bench-simulation use case weighed explicitly
- [ ] #2 If it should not count: the bBoilerState path excludes loopback the same way otDirectBoilerPresent() does, and the two cannot drift apart again
- [x] #3 If it should count: the asymmetry with otDirectBoilerPresent() is documented at both sites so the next reader does not file this again
- [x] #4 Build green for the combo target and evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-20 decision (maintainer, interactive): loopback traffic DOES count as boiler presence for state.otBus.bBoilerState. Rationale: loopback (GW=L) exists to make the stack behave as if a boiler were attached, and whoever enables it knows the boiler is synthetic. The exclusion in otDirectBoilerPresent() is not a general rule that loopback is not a boiler; it breaks a specific self-reference (the SAT simulation availability gate would otherwise disable itself the moment simulation raised the flag). So the two sites differ by design. Outcome: AC #3, document the asymmetry at both sites; no behaviour change.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Decision recorded and documented at both sites; no behaviour change.

Loopback (GW=L) traffic counts as boiler presence for state.otBus.bBoilerState, by maintainer decision on 2026-09-20. otDirectBoilerPresent() keeps its loopback exclusion because that function gates SAT simulation and would otherwise disable itself; that is a local self-reference fix, not a general rule. The asymmetry is now explained in a comment at each site (OTGW-Core.ino at the tBoilerLastSeen stamp, OTDirect.ino above otDirectBoilerPresent()), each pointing at the other, so the next reader does not file this again.

AC #2 is the not-taken branch and stays unchecked by design.

Gates: build.bat --target esp32-combo, two SUCCESS steps, fresh artifact, flash 81.3 percent, binary size identical to alpha.365 (comment-only). evaluate.py --quick 76 checks, 0 failed, 1 pre-existing warning. Committed with the bump hook disabled: no firmware behaviour changed, so there is nothing for a tester to A/B; the lines ride into the next real prerelease bump.
<!-- SECTION:FINAL_SUMMARY:END -->
