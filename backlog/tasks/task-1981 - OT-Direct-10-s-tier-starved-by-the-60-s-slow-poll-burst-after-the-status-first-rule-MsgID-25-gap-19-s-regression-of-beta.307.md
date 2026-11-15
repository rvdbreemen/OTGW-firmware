---
id: TASK-1981
title: >-
  OT-Direct: 10 s tier starved by the 60 s slow-poll burst after the
  status-first rule (MsgID 25 gap 19 s, regression of beta.307)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-15T16:38:00Z'
updated_date: '2026-11-15T17:47:00Z'
labels:
  - otdirect
  - regression
dependencies: []
priority: high
ordinal: 1010000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Regression of fff3604f4 (TASK-1804 Bug 4, MsgID 0 first). Status-first shifts the 10 s tier by 100 ms; every 60 s the slow tier (~80 IDs) comes due first and the round-robin pointer is past the 10 s tier, so MsgID 25 (SAT flow) waits for the whole burst: 19.2 s gap once a minute on OT-Direct without PM=25. Found by 95 (bisect), red: tests/test_sat_no_return_startup.py OtDirectPm, test_sat_lowload_contract ll018h. Classic unaffected.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 test_sat_no_return_startup OtDirectPm and test_sat_lowload_contract ll018h red on origin, green on the fix
- [ ] #2 test_otd_status_priority and all test_otd_* stay green; host slice green
- [ ] #3 Bench: MsgID 25 read interval on .61 at most ~10.1 s over 3 minutes (no 19 s gap)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 d3: fix in scheduleMasterRequest(): two round-robin passes, first only entries with an interval shorter than OT_SLOW_INTERVAL_MS, then all; status-first (Bug 4) stays. Red on lane-1804 HEAD 2e80ebe (OTDirect.ino of HEAD via OTGW_OTD_INO): test_sat_no_return_startup 2 failed (MsgID 25 gap 19.2 s; PM=25 in the burst), green on the fix 20/20. The burst latency test was a characterization of the old behaviour that asked to be tightened when PM=25 is served inside one interval: now asserts max latency <= 1150 ms (66 ms measured), renamed test_pm25_goes_out_within_a_communication_interval_also_in_the_slow_poll_burst; ll018h updated to the new name, 1 passed. test_otd_status_priority 2 passed. Pre-1954 OTDirect.ino (fff3604f4^): OtDirectPm 5/5 green, confirms the regression point.

2026-11-15 18:2x d3: the full sweep found two harnesses that slice the real scheduler without OT_SLOW_INTERVAL_MS: test_otd_mode_switch_queue_reset (own prelude, stub added) and test_otd_offline_retry (imports the real constants; now skips one the prelude already defines, else a redefinition). Gates on lane-1804 + fix (70fcfba16 rebased on origin, 0 lines difference from before the rebase): test_otd_* 63 files green (in two parts plus the 8 scheduler harnesses after the stubs), test_sat_no_return_startup 20/20, ll018h green, test/host override_reply 41/41, evaluate --quick 86/0, tests/test_evaluate.py Ran 80 OK. Full test_sat_lowload_contract runs detached (longer than 10 min).

2026-11-15 d3: full test_sat_lowload_contract (detached, 15 min): 1 failed, 32 passed, 1 skipped, 1 xfailed. The one failure was an UNEXPECTED SUCCESS of ll018j (PM=25 within one communication interval in the full schedule), marked open_decision with question OTD-PM25 that was never asked (not in docs/sat/open-questions.md). It is OT-Direct transport, not SAT control, so the PIC decides: SendPriorityMsg (gateway.asm 6.6 :3436-3441) is checked before the alternatives and repeats until answered (HandleResponse :2431-2434). ll018j reclassified to contract (task TASK-1981): red on lane-1804 HEAD OTDirect.ino, green on the fix (ll018h + ll018j 2 passed).
<!-- SECTION:NOTES:END -->
