---
id: TASK-3034
title: 'SAT: false ''No ignition'' events while the tolerance band holds CH off'
status: To Do
assignee: []
created_date: '2026-11-22T14:50:00Z'
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
- [ ] #2 Fix passes _sat_tolBandOff to the satChEnableDecide() call that feeds satDiagEventsStep (the same input satSendCH uses)
- [ ] #3 python evaluate.py --quick shows no new FAIL
- [ ] #4 Bench on .63 with a SAT tolerance-band pause shows no no_ignition event
<!-- AC:END -->
