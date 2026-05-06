---
id: TASK-367
title: >-
  chore(backlog): append erratum on TASK-342/346/349/351 Final Summaries and
  remove plan-file references
status: Done
assignee:
  - '@claude'
created_date: '2026-04-21 07:47'
updated_date: '2026-04-21 17:04'
labels:
  - backlog
  - hygiene
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Phase 3B HIGH+LOW: three Final Summaries misrepresent shipped behaviour (TASK-342 claims all Status call sites wrapped, VH missing; TASK-349+351 claim NTP/uptime preconditions that don't exist; TASK-346 claims doTaskEvery60s call site that TASK-350 moved). TASK-348/349/350/351 Descriptions leak plan-file reference expressive-growing-yao.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 TASK-342 Final Summary appended with erratum noting VH publishers (publishMasterStatusVHState/publishSlaveStatusVHState/publishStatusVHBitMQTT) are not wrapped
- [x] #2 TASK-349 and TASK-351 Final Summaries appended with erratum: NTP sync and uptime>3600 are NOT currently enforced in startDiscoveryVerification
- [x] #3 TASK-346 Final Summary appended with correction: post-TASK-350, sendMQTTheapdiag runs under doTaskMinuteChanged hourFlag instead of doTaskEvery60s
- [x] #4 TASK-348/349/350/351 Descriptions: remove the See plan file trailing sentence
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Append erratum to TASK-342, 346, 349, 351 Final Summaries
2. Edit Descriptions of TASK-348, 349, 350, 351 to remove plan-file references
3. Verify via backlog task <id> --plain
4. Check ACs on TASK-367
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Errata appended to TASK-342/346/349/351 Final Summaries via --append-final-summary. Plan-file references removed from Descriptions of TASK-348/349/350/351 via -d. All edits verified via backlog task <id> --plain. Preserved surrounding prose verbatim; only the plan-file phrase was stripped.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Backlog hygiene pass for 1.4.1: appended erratum blocks to four Final Summaries where the shipped behaviour diverged from what the summary claimed, and stripped leaked private plan-file references ("expressive-growing-yao") from four Descriptions.

Errata appended:
- TASK-342: clarified that only CH Master/Slave status publishers were wrapped by beginStatusBurst/endStatusBurst; VH publishers (publishMasterStatusVHState / publishSlaveStatusVHState / publishStatusVHBitMQTT) were missed and are being closed by TASK-354.
- TASK-346: corrected call-site attribution; sendMQTTheapdiag now runs under if(hourFlag) inside doTaskMinuteChanged per ADR-064 (unified dispatcher), not inside doTaskEvery60s as originally written.
- TASK-349: noted that NTP-sync and uptime>3600 preconditions were NOT enforced in startDiscoveryVerification as originally shipped; closed by TASK-359 (now Done).
- TASK-351: same NTP/uptime precondition erratum as TASK-349; cross-referenced TASK-359.

Descriptions cleaned:
- TASK-348: removed "(see plan: expressive-growing-yao)" inline reference; preserved "Part of the discovery verification + auto-heal plan. Ships first as lowest-risk cleanup."
- TASK-349: removed "See plan file expressive-growing-yao and"; preserved "See ADR-062."
- TASK-350: removed trailing "See plan file expressive-growing-yao." sentence.
- TASK-351: removed trailing "See plan file expressive-growing-yao." sentence.

All edits via backlog task edit CLI (no direct file writes). No AC changes, no status changes on the legacy tasks. Verified via backlog task <id> --plain after each edit.
<!-- SECTION:FINAL_SUMMARY:END -->
