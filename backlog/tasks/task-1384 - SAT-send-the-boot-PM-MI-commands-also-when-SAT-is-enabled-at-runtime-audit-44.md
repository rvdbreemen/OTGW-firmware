---
id: TASK-1384
title: >-
  SAT: send the boot PM/MI commands also when SAT is enabled at runtime (audit
  #44)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T10:39:00Z'
updated_date: '2026-11-04T07:08:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies:
  - TASK-1412.3
  - TASK-1412.5
references:
  - TASK-1412
ordinal: 523000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From TASK-1273 and sat-port-audit-2026-10-24.md #44: the PM/MI commands sent at boot are not sent when SAT is switched on later. Relates to entry 45 (MI=500 for boilers without return temperature) and TASK-1371.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Enabling SAT at runtime sends the same PM/MI set as boot (host test or telnet transcript, red on HEAD)
- [x] #2 Sergeant D's OK recorded in docs/sat/sergeant-d-corrections.md before implementation (CLAUDE.md SAT rules)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 execution sequencing: reused by TASK-1412. Required prerequisites are encoded as dependencies; existing status, queue ordinal, decisions and approval requirements are preserved. Do not resume affected control changes before the scoped approved contract and red tests. Later integration, simulation, bench acceptance and field evidence are tracked by the epic; this metadata update is not implementation or validation.

2026-10-30 coordinator: skipped in this cycle: AC2 needs Sergeant D's OK first (not recorded). Note TASK-1371 already moved MI=500 to satMiStep() after satRestartStep(), which also runs when SAT is enabled at runtime; the PM chain is still boot-only. Question to queue.

2026-11-04 Robert decided (maintainer decision, entries 257-262; Sergeant D informed msg 1567417361798144021, may object). See docs/sat/sergeant-d-corrections.md.

2026-11-04 implemented (sonnet agent; lead reran on the integration tree): see CHANGELOG [Unreleased] and docs/sat/firmware-sat-spec.md. Host tests red on old, green on fix: test_sat_pwm_start_limit 26 OK (1515), test_pic_pm_chain 40 OK (1384), test_sat_no_return_startup 20 OK (1412.12), overshoot_sustain 62 OK, no_return_ignition 19 OK, evaluate 0 failed. Shipped together as beta.66 (one SATcontrol.ino commit for TASK-1515, TASK-1384, TASK-1412.12). Bench pending.

AC1/AC2 superseded by Robert's decision (entry 261, 2026-11-04, maintainer decision, Sergeant D informed): instead of sending PM at runtime enable, the boot PM chain always runs on PIC boards, so MsgID 3 is known before any later enable.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PIC boards run the boot PM chain (PM=3/15/48) at every boot whatever SAT is (entry 261). Evidence: tests/test_pic_pm_chain.py old: PIC with SAT disabled sends nothing, fix PM=3; bench beta.66 (.63, SAT and simulation off, reboot): cmdq_prio_slot 3, cmdq_sent 9, echo 7 shortly after boot; settings restored.
<!-- SECTION:FINAL_SUMMARY:END -->
