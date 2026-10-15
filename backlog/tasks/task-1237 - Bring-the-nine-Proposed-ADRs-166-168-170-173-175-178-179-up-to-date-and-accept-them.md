---
id: TASK-1237
title: >-
  Bring the nine Proposed ADRs (166, 168, 170-173, 175, 178, 179) up to date and
  accept them
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-15 15:46'
updated_date: '2026-10-15 16:01'
labels:
  - adr
  - docs
dependencies: []
ordinal: 375000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Acceptance sweep 2026-10-15. Code-vs-ADR verification by three read-only agents; maintainer decisions: accept 168/170/171/172/175/178 after small fixes; rewrite 173 around the shipped v2.js client design and accept; scope 179 to OT-Direct (PIC follow-up TASK-1236, MsgID 0 question moved to its own task) and accept; accept 166 then mark it superseded by ADR-191.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each ADR's stale references and claims corrected per the verification report
- [x] #2 adr-lint --strict docs/adr reports no FAIL for these nine ADRs
- [x] #3 168, 170, 171, 172, 173, 175, 178, 179 Accepted via adr accept --confirm
- [x] #4 166 Accepted then Superseded by ADR-191 (bidirectional)
- [ ] #5 Docs-only commit pushed to origin/dev
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Acceptance sweep of all nine Proposed ADRs (2026-10-15). Three read-only agents checked each decision against current dev code (file:line), then the records were refreshed. Evidence per AC: (1) stale refs fixed: 168 line refs + third mute path + TASK-1129 cause; 170 ADR-167 Accepted + bDiscoveryAutoVerify note; 171 Enforcement corrected, ADR-182 related; 172 v2.js context, unsigned GCRA lead (TASK-1037); 173 rewritten around the shipped v2.js client with accepted gaps (no jitter/backoff); 175 OT-Direct baseline 369 keys/230 topics, explicit --topics telnet; 178 open question answered (hold <=10 min then drop), downgrade wording, POST shape gap; 179 scoped to OT-Direct, Q1 -> TASK-1238, Q2 corrected gateway.asm reading -> TASK-1236. (2) adr accept ran strict lint incl. clarity per ADR: all passed; adr-readiness --all-proposed reports 0 records. (3)+(4) adr accept --confirm for 168/170-173/175/178/179; 166 accepted then adr supersede --by ADR-191. README index brought up to date incl. the missing ADR-185..192 (tests/test_adr_governance.py 15/15 OK, failed before on HEAD). Spin-off tasks: TASK-1236 (probable PIC CS lapse under SAT, high), TASK-1238 (MsgID 0 CH-enable in OT-Direct gateway mode).
<!-- SECTION:FINAL_SUMMARY:END -->
