---
id: TASK-627
title: 'feat-2.0.0: port TASK-626 — MQTT proxy-answer routing fix'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-19 16:11'
updated_date: '2026-05-25 23:00'
labels:
  - mqtt
  - routing
  - adr-075
  - proxy-answer
  - feat-2.0.0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of TASK-626 / ADR-075 to the 2.0.0 feature line (feature-dev-2.0.0-otgw32-esp32-sat-support). Logically identical change at feature-line line numbers: OTGW-Core.h:559, OTGW-Core.ino detection ~4096-4109, master-topic gate OTGW-Core.ino:1293, MQTTstuff.ino:1658-1660. The 2.0.0 worldview ADR is ADR-096 (independent numbering); a sibling ADR in the feature worktree's docs/adr cross-references dev ADR-075 and supersedes ADR-096. Single-worktree container: delivered on a sibling claude branch off origin/feature-dev-2.0.0-otgw32-esp32-sat-support via draft PR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Feature-line OpenthermData_t gains byte bAnswerOverride; default 0; set 1 only on detected (B,A) override A
- [ ] #2 Proxy A publishes to _thermostat+_boiler+canonical; answer-override A unchanged (_thermostat only)
- [ ] #3 Sibling ADR authored in feature docs/adr (next free number), supersedes ADR-096, cross-references ADR-075
- [ ] #4 Build green for the relevant 2.0.0 target; evaluator no new failures
- [ ] #5 Version prerelease bumped on the feature line in the same commit as the firmware change
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Port was already completed as TASK-628 in the 2.0.0 worktree (PR #600, commit 41423f78, merged 2026-05-19). ADR-103 authored, all 5 code changes present, build+eval green, prerelease bumped alpha.39. No additional work needed.
<!-- SECTION:FINAL_SUMMARY:END -->
