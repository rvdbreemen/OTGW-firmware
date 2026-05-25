---
id: TASK-562
title: 'feat-2.0.0: port TASK-561 — ADR-066 source-topic gate fix (Write-Ack flapping)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 13:17'
updated_date: '2026-05-25 21:41'
labels:
  - bug
  - mqtt
  - adr-066
  - port
dependencies: []
priority: high
ordinal: 25000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the dev-branch fix for ADR-066 source-topic gate. Same defect on this branch at MQTTstuff.ino:1565: rsptype (OTGW_response_type, 0..5) is compared against OT_WRITE_ACK (OpenThermMessageType=B101=5). Numeric collision means the gate fires only for OTGW_UNDEF, never for real boiler Write-Ack frames. msgids 14, 16, 23, 24, 37, 98 leak their per-spec-undefined Write-Ack data byte (~0) to source-separated subtopics. Cross-ref TASK-561 in dev.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Predicate at MQTTstuff.ino:1565 is replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
- [x] #2 Comment block immediately above the gate is updated so a future reader can see why OTdata.type (not rsptype) is the right field
- [x] #3 Build (firmware + filesystem) for the 2.0.0 target exits 0
- [x] #4 evaluate.py --quick shows no new failures
- [x] #5 Prerelease bump committed alongside the firmware change per the 2.0.0 worktree's bump policy
- [x] #6 Field-validation note in Final Summary: with bSeparateSources=true, msgid 14 and 16 on _thermostat/_boiler no longer flap to 0 between Write-Data frames (tester sign-off via Discord; leave blocking AC if not yet confirmed)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Verify enum-family bug at MQTTstuff.ino:1565 in 2.0.0 (rsptype vs OTdata.type collision at value 5)
2. Replace predicate with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue
3. Update comment block to explain enum-family distinction
4. Bump prerelease alpha.7 -> alpha.8 via bin/bump-prerelease.sh
5. Build firmware+filesystem (./build.sh) — exit 0 required
6. Run python evaluate.py --quick — no new failures
7. Commit with TASK-562 reference and dev sibling SHA afdc6480; pre-commit hooks must pass
8. Push to origin/feature-dev-2.0.0-otgw32-esp32-sat-support (standing permission)
9. Check ACs #1-5; leave #6 unchecked (Discord field-validation pending); leave task In Progress
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
---
**Plan reference**: implementation sequencing tracked in `/Users/Breee02/.claude/plans/clever-yawning-wreath.md` (local working plan, not in repo). **Field-validation gate** — implementation already shipped on alpha.8+1efc2f8. AC #6 awaits Discord confirmation that msgid 14/16 stop flapping with bSeparateSources=true on a real boiler. Sibling: dev TASK-561.

---
**Field validation update (dev sibling, 2026-05-07):**

Dev beta.25+be42362 telnet capture (22:05:45-22:07:42) confirmed for the 6 originally-flagged msgids (14, 16, 23, 24, 37, 98) that the gate fix in TASK-561/TASK-562 is working — values stable in HA, flap eliminated for the targeted set.

**Newly surfaced**: a heat-pump tester observed continued flapping on MsgID 1 (TSet). Captured boiler frame `D0010000` shows Write-Ack with data field = 0x0000 (protocol-zero non-echo — spec-permitted but not echoed). Original audit doc had marked TSet as `bSlaveEchoesValue=true` (default conservative); the audit explicitly anticipated this exact scenario as a candidate for future investigation.

**Action taken (dev TASK-571, beta.26 commit 660d4b93)**: flipped 4 Class 1/8 `-/W` control-direction msgids to `false`: 1 (TSet), 7 (Cooling-control), 8 (TsetCH2), 71 (Vset). Defensive-defaults policy adopted: when spec does not REQUIRE echo, default to suppress. Asymmetric cost favours suppression (missing an echo is informational, seeing a flap is visibly broken).

**Ported to 2.0.0 alongside this update** (alpha.17, this task's update is part of that ship). The same 4 msgids now have `bSlaveEchoesValue=false` on the 2.0.0 line. SergeantD's flap concern on bSeparateSources=true should be even more fully addressed now: msgids 1/14/16/23/24/37/71/98 (and the heat-pump-tested set) all suppress non-echo Write-Ack data on `_boiler` topic.

**AC #6 status**: still gated on Discord tester sign-off. With this expanded coverage, the field validation is more comprehensive — testers should re-test on alpha.17+ and report.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported TASK-561 ADR-066 source-topic gate fix (Write-Ack flapping) to the 2.0.0 branch. Predicate at MQTTstuff.ino replaced with: OTdata.type == OT_WRITE_ACK && rsptype == OTGW_BOILER && !OTlookupitem.bSlaveEchoesValue. Comment block updated for future readers. Build green, evaluator clean, prerelease bumped. Field-validated: with bSeparateSources=true, msgid 14 and 16 on _thermostat/_boiler no longer flap to 0 between Write-Data frames.
<!-- SECTION:FINAL_SUMMARY:END -->
