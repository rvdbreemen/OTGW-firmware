---
id: TASK-3152
title: >-
  SAT: a fresh 0.0 return temperature (MsgID 28 FreshZero) counts as missing
  return, so the flow-based start-up hold applies (Alexwijn/SAT#216 analogue)
status: To Do
assignee: []
created_date: '2026-11-28T10:17:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1197000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D asked us to pay attention to https://github.com/Alexwijn/SAT/issues/216 (register entry 529, #dev-sat-mqtt message 1576179347008524500). Evidence from poc-dev at 313c4ba (not re-checked by otgw-docs): satPwmFlameOffHold SATcontrol.ino:1249-1251; satFeedClassIsFresh counts FreshZero as fresh (SATboilerFeed.h:113, :119-122); satReturnFresh :3043; satGetReturnTemp :3085; satNoReturnTemp :3108-3112; guard in satApplyPWM :1698 and the continuous start :2511 (returns the previous CS, the burner is not blocked). Fix idea: treat FreshZero as a missing return temperature when MsgID 28 never gave a non-zero value, so satNoReturnTemp() holds and the flow-based hold applies; also check the delta flow-return statistics. After the SAT mode-switching beta, not in the current build. Our pin 7beaaf5 is thermo-nova 5.0.0; the issue is on a later upstream line. Ground-truth rule: a second reader must open the same lines before any claim to Sergeant D or on GitHub.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A host test on satNoReturnTemp with a fresh 0.0 return (MsgID 28 FreshZero) fails on 313c4ba and passes after the fix
- [ ] #2 The flow-despite-zero-return statistics (delta flow-return) are checked and the result is written in the task notes
- [ ] #3 Bench check on a boiler that reports 0.0 as return temperature, if one exists; otherwise stated as not verified
<!-- AC:END -->
