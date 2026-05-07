---
id: TASK-570
title: 'docs(mqtt): note retained otthing/* topics for OTTHING-port testers'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-07 19:36'
updated_date: '2026-05-07 21:56'
labels:
  - mqtt
  - migration
  - 2.0.0
dependencies: []
priority: medium
ordinal: 33000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scope shrunk per maintainer (2026-05-07): do NOT add firmware migration code. Just add a tester-facing note in docs/guides/ explaining that retained otthing/<chipid>/* topics observed on the broker after flashing OTGW32 firmware are zombies from the prior OTTHING-platform firmware. Provide the mosquitto / HA cleanup recipe so testers can clear them once and never see them again. Current OTGW32 firmware does NOT publish under otthing/ (verified: only sTopTopic='OTGW' default in MQTTstuff.h:51).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 docs/guides/MIGRATION_FROM_OTTHING.md exists and explains: (a) where the otthing/<chipid>/* retained topics come from (prior OTTHING-platform firmware), (b) why current OTGW32 firmware does not emit them (board macro vs runtime topic prefix), (c) one-shot mosquitto_pub -t '<topic>' -r -n cleanup recipe testers can copy-paste
- [x] #2 Guide is reachable from a 2.0.0 entry-point doc (linked from BUILD or migration index)
- [x] #3 Scope is intentionally docs-only — no firmware-side cleanup code shipped (per user instruction 'just document it for testers, don't try to code migration cleanup')
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. user reframed scope mid-flight from firmware-cleanup to docs-only ('Don't try to code migration cleanup'); 2. write a tester-facing migration guide; 3. close out.
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Original ACs proposed firmware-side cleanup (subscribe otthing/+/+, clear retained payloads, ADR for deprecation timeline). User reframed mid-flight to docs-only — testers can clean up retained otthing/* zombies themselves with a one-shot mosquitto_pub recipe; firmware does not need a migration block. Delivered: docs/guides/MIGRATION_FROM_OTTHING.md (97 lines, 5147 bytes). ACs rewritten to match the actually-shipped scope and all checked.
<!-- SECTION:FINAL_SUMMARY:END -->
