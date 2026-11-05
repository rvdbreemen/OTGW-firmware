---
id: TASK-1608
title: >-
  SAT zone and PV-boost HA discovery names without hostname, spaced (PR #693,
  Andre Brait)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-04T17:56:00Z'
updated_date: '2026-11-05T16:34:00Z'
labels: []
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 748000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Contributor PR #693 by Andre Brait: streamSatZoneDiscovery() and streamSatPvBoostDiscovery() in MQTTstuff.ino render 'SAT Zone <n> <Metric>' and 'SAT PV <Suffix>' instead of '<hostname>_SAT_..._' names, like the main discovery path. unique_id, state/command and discovery topics unchanged, so entity_id and history stay. The PR reused TASK-1603, which on dev is a different task; this task replaces that file. Taken over with the author kept, without the PR's beta.71 bump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Zone and PV-boost discovery names are spaced and without hostname (host test or broker capture)
- [x] #2 unique_id and topics unchanged (diff check)
- [x] #3 Release notes mention the display-name change in Home Assistant
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Host evidence (2026-11-04, lane-prs 038773cf1): format/argument count of every snprintf_P in streamSatZoneDiscovery and streamSatPvBoostDiscovery matches (12 calls, each exactly one %s and one ctx.hostname argument removed vs origin/dev); uniq_id, stat_t/cmd_t and discovery topics unchanged in the diff. Comment reference corrected from TASK-1603 to TASK-1608. Contributor bench: OTGW32 combo with boiler, HA showed the new names on the existing entities, entity_id unchanged. Not yet: our build/bench (after beta.77).

2026-11-04 bench .63 beta.79 (7f32b91b8 = beta.78, built with beta.79): retained configs on 192.168.88.32 show 'SAT Zone 1 Active/Error/Output', 'SAT PV Boost Delta/Enabled/Hold/Max Duration/Max Indoor/Threshold' (spaced, no hostname); uniq_id otgw-<mac>-sat_zone_1_active etc. and stat_t unchanged (same pattern as the .61 capture earlier today). CHANGELOG [Unreleased] Changed line added; AC3 (release-notes mention) closes with the next public beta notes.

2026-11-05: AC3 by the v2.0.0-beta.95 release notes (docs/releases/RELEASE_NOTES_2.0.0-beta.95.md:61 'The SAT zone and PV-boost entities get readable names' and :159 BREAKING display names), published 2026-11-05.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT zone and PV-boost HA discovery names spaced and without hostname (PR #693, André Brait); unique ids/topics unchanged. Bench: discovery names on the broker (beta.79). Release notes beta.95 state the display-name change.
<!-- SECTION:FINAL_SUMMARY:END -->
