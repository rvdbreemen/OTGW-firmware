---
id: TASK-1795
title: >-
  Audit: OT-Direct (OTDirect.ino) against the PIC gateway firmware (otgw-6.6
  gateway.asm), behaviour parity per area
status: Done
assignee:
  - '@claude'
created_date: '2026-11-11T06:11:00Z'
updated_date: '2026-11-11T07:08:00Z'
labels:
  - review
  - otdirect
milestone: 2.0.0
dependencies: []
ordinal: 890000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-11: review and audit the OT-Direct implementation and compare it with the PIC firmware in assembler (other-projects/otgw-6.6/gateway.asm). Read-only multi-agent review, one area per reviewer, each finding with file:line on both sides and a skeptic pass. Builds on the 2026-04-27 parity audit. Output: verified findings -> one backlog task per real divergence (bug, intended difference to document, or open question).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each area (gateway relay + overrides, command interface, message scheduling/slots, bus timing/errors/line state, boiler-cache/answer generation) reviewed with file:line citations on both sides
- [x] #2 Every finding verified by the coordinator against both sources; rejected findings listed with reason
- [x] #3 Each confirmed divergence filed as its own task (bug / document-as-intended / question for Robert), deduped against existing tasks
- [x] #4 Summary report in docs/reviews/ with the per-area verdict
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: five areas reviewed by four read-only agents (A gateway relay, B command interface, C+D scheduling/timing, E boiler cache), each with file:line on both sides (OTDirect.ino / gateway.asm) and a skeptic pass; reports in docs/reviews/2026-11-11_otdirect-pic-parity-audit/. AC2: coordinator verified on both sides: BC1 banner OTDirect.ino:3271, BC9 MM=- :2976, A3 mandatory IDs :1616-1633 vs gateway.asm:3383, CD Bug 3 OpenTherm.cpp:443 + :294-302 vs OTDirect.ino:2751; refuted findings are listed per report. AC3: TASK-1797..1808 filed (dedupe vs 1750/1751/1707/1733/1759/1138); Robert decided A1 (TASK-1805, ADR-229 reserved) and the setback (TASK-1806); E-cand 4 and U1 wait for Robert. AC4: REVIEW.md with the per-area verdict (commits 07ec6c61b, d16a09f0a). Not verified: everything is code-read only, no bench/capture behind the candidates.
<!-- SECTION:FINAL_SUMMARY:END -->
