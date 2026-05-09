---
id: TASK-596
title: 'docs: update documentation for changes since v1.5.0-fix'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-09 00:04'
labels:
  - docs
  - update-docs
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sequential doc update pass after v1.5.0 release. PREV_TAG: v1.5.0-fix. Key changes: pure JIT MQTT discovery (ADR-073, supersedes ADR-041), SAT climate_attributes wired to HA json_attributes_topic (TASK-589), orphaned sat/pressure_health_attr removed (TASK-590), flash_otgw.bat fixes. New ADRs: 070 (MQTT source topic sibling suffix), 071 (MQTT discovery topic sibling suffix), 072 (HA discovery friendly name format), 073 (JIT HA discovery smart reconnect). More than 6 subsystems changed; treating all docs as in scope.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 API documentation (docs/api/MQTT.md, openapi.yaml, README.md, WEBSOCKET_FLOW.md, WEBSOCKET_QUICK_REFERENCE.md) updated to reflect JIT discovery, SAT changes, and WebSocket behavior
- [ ] #2 ADR README (docs/adr/README.md) updated with ADR-070, ADR-071, ADR-072, ADR-073 entries
- [ ] #3 Cleanup phase complete: RELEASE_NOTES_1.5.0.md and RELEASE_GITHUB_1.5.0.md moved to docs/releases/, misplaced files resolved
<!-- AC:END -->
