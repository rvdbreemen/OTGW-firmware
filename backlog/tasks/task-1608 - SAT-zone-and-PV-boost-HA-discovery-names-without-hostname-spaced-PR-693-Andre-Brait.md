---
id: TASK-1608
title: >-
  SAT zone and PV-boost HA discovery names without hostname, spaced (PR #693,
  Andre Brait)
status: To Do
assignee: []
created_date: '2026-11-04T17:56:00Z'
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
- [ ] #1 Zone and PV-boost discovery names are spaced and without hostname (host test or broker capture)
- [ ] #2 unique_id and topics unchanged (diff check)
- [ ] #3 Release notes mention the display-name change in Home Assistant
<!-- AC:END -->
