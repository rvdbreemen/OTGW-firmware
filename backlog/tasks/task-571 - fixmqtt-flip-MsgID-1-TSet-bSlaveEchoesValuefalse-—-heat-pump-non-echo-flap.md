---
id: TASK-571
title: >-
  fix(mqtt): flip MsgID 1 (TSet) bSlaveEchoesValue=false — heat-pump non-echo
  flap
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 20:06'
updated_date: '2026-05-22 06:41'
labels:
  - bug
  - mqtt
  - adr-066
  - 2.0.0-port-needed
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field validation on dev beta.25+5153537 (2026-05-07) confirmed TASK-561 ADR-066 fix works for the 6 flagged msgids (14, 16, 23, 24, 37, 98) — values stable in HA. EXCEPT TSet (MsgID 1) on the user's heat pump still flaps between override values and 0. Root cause: the heat pump's Write-Ack data field returns 0 instead of echoing the master value (per OT v4.2 spec ambiguity for MsgID 1; some Class 1 controllers do this, others echo). The audit doc (docs/api/MQTT-message-id-echo-audit.md lines 27, 140-141) explicitly anticipated this: 'Class 1 / Class 8 control writes that may be non-echo on certain boilers' — TSet listed as primary candidate for future investigation. Fix: flip MsgID 1's bSlaveEchoesValue from true to false in OTmap[] (OTGW-Core.h around line 354). The existing is_value_valid_for_master_topic + publishToSourceTopic gates will then suppress the protocol-zero Write-Ack data on canonical and _boiler topics for MsgID 1 the same way they do for 14/16/23/24/37/98.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 OTGW-Core.h OTmap[] entry for MsgID 1 (TSet) has bSlaveEchoesValue changed from true to false
- [x] #2 docs/api/MQTT-message-id-echo-audit.md updated: MsgID 1 row's bSlaveEchoesValue column changed to false; reason column updated with the field evidence (heat-pump tester report 2026-05-07); 'Future extensions' candidates list updates to mark TSet as confirmed and removes it from the candidate list
- [x] #3 python build.py --firmware exits 0 on dev
- [x] #4 python evaluate.py --quick — no new failures
- [x] #5 Prerelease bump committed alongside (beta.25 -> beta.26)
- [ ] #6 Field validation on beta.26+: tester confirms TSet boiler value no longer flaps between override and 0 with bSeparateSources=true (deferred per CLAUDE.md self-verification policy)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. flip OTmap[] entries for msgid 1, 7, 8, 71 (already done in beta.26); 2. update audit doc; 3. ship and bump prerelease; 4. wait on field validation.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-05-22: code change shipped on dev (commit 660d4b93) and made it into main/v1.5.0-fix2. AC#6 (field validation by tester) is the residual blocker. No new TSet-flap reports since 1.5.0 stable was tagged. Blocked on positive field signal — re-open if a tester re-tests on 1.6.0-beta.16+ and confirms or denies.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Class 1 / Class 8 control-write flag flips applied to msgid 1 (TSet), 7 (CoolingControl), 8 (TsetCH2), 71 (ControlSetpointVH) — all four bSlaveEchoesValue flipped from true to false per defensive-defaults policy. OTmap[] in OTGW-Core.h:341, 347, 348, 411 confirmed. Audit doc docs/api/MQTT-message-id-echo-audit.md row 27 (TSet) updated with field-tester evidence; candidate list cleared. Bump beta.25 → beta.26 landed in commit 660d4b93. AC #6 (tester confirms TSet boiler value no longer flaps with bSeparateSources=true post-fix) remains gated on Andre's re-flash and re-observation; not self-verifiable from our side.
<!-- SECTION:FINAL_SUMMARY:END -->
