---
id: TASK-3034
title: 'SAT: false ''No ignition'' events while the tolerance band holds CH off'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T14:50:00Z'
updated_date: '2026-11-25T06:38:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1142000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Evidence: marceld91d beta.410 capture (OTGW-field-data/marceld91d/2026-11-22_beta.410_capture/): 10 of 10 SATEVENT no_ignition=1 fired inside 'Heating paused (CH off, PWM stays active)' windows; CH enable bit 0 in all 60 status frames in the 180 s before each. Cause (read in code by the coordinator, c6f0302 and origin/dev 72e8afa74): the satDiagEventsStep input d.chAsked = !coldCutoff && satChEnableDecide(pidOutput, satColdThreshold(), pwmDrivesSetpoint, pwmOn) at SATcontrol.ino:8532 (dev :8625) omits _sat_tolBandOff, while satSendCH at :8555 (dev :8648) passes it (SATchEnable.h returns 0 for bandOff). So the no-ignition diagnostic believes CH is asked while the tolerance band (ADR-250/ADR-253) holds it off. Diagnostic only (TASK-1882): no control action changes. No code until the token budget hold ends (Wed 25 Nov).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Host test drives the tolerance-band-off path and shows a no_ignition event on the old code and none on the fix
- [x] #2 Fix passes _sat_tolBandOff to the satChEnableDecide() call that feeds satDiagEventsStep (the same input satSendCH uses)
- [ ] #3 python evaluate.py --quick shows no new FAIL
- [ ] #4 Bench on .63 with a SAT tolerance-band pause shows no no_ignition event
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 fix: SATcontrol.ino:8625 d.chAsked now calls satChEnableDecide(..., _sat_tolBandOff), the same arguments as the satSendCH call at :8648. Tests (tests/test_sat_diag_events.py, class ChAskedFollowsTheBand, 2 tests): RED on origin/dev (2 failures: the diag call lacks _sat_tolBandOff), GREEN on the fix (14 tests OK); test_sat_tolerance_band OK. HONEST LIMIT: the call sites are inside satControlLoopRun(), not compilable on the host, so AC 1 is covered STRUCTURALLY (source read), not by driving the tick; the live path is AC 4 on the bench (.63, a band pause must show no no_ignition event). AC 1 therefore stays unticked.

2026-11-25 second field instance: will_85903 beta.418, no_ignition=1 at 08:13:28 (cleared 08:19:28) while SAT itself held CH off (OTGW-field-data/will_85903/2026-11-23_beta.418_capture/analysis_findings.txt, via the coordinator); confirmed on origin/dev: diag d.chAsked (SATcontrol.ino:8625) omitted _sat_tolBandOff, the real send passes it. Fix applied again on the beta branch.
<!-- SECTION:NOTES:END -->
