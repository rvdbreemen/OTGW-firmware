---
id: TASK-596
title: 'docs: update documentation for changes since v1.5.0-fix'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-09 00:04'
updated_date: '2026-05-09 00:12'
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
- [x] #1 API documentation (docs/api/MQTT.md, openapi.yaml, README.md, WEBSOCKET_FLOW.md, WEBSOCKET_QUICK_REFERENCE.md) updated to reflect JIT discovery, SAT changes, and WebSocket behavior
- [x] #2 ADR README (docs/adr/README.md) updated with ADR-070, ADR-071, ADR-072, ADR-073 entries
- [x] #3 Cleanup phase complete: RELEASE_NOTES_1.5.0.md and RELEASE_GITHUB_1.5.0.md moved to docs/releases/, misplaced files resolved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 done: MQTT.md updated with JIT discovery semantics (ADR-073), SAT topics section added (climate_attributes, pressure scalars, pressure_health_attr removal). README.md one correction on POST /api/v2/discovery/verify. openapi.yaml/WEBSOCKET docs unchanged (version-bump only). Flag: mqttharebootdetection may be no-op post-ADR-073, noted in docs but no removal decision needed yet.

AC2 done: ADR README updated — ADR-041 entry added (marked superseded by ADR-073), ADR-073 entry added in Integration section and Decision Timeline. ADR-070/071/072 were already present.

AC3 done: RELEASE_NOTES_1.5.0.md and RELEASE_GITHUB_1.5.0.md moved from root to docs/releases/. RELEASE_NOTES/GITHUB_1.3.3 and 1.3.4 archived to docs/releases/archive/ (keeping 4 newest). docs/ root clean.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Updated documentation for changes since v1.5.0-fix (beta.1-3 cycle). MQTT.md updated with JIT discovery semantics per ADR-073 (split boot vs. JIT behavior, SAT topics section added, pressure_health_attr removal noted). docs/api/README.md corrected discovery/verify endpoint description. docs/adr/README.md: ADR-041 entry added as superseded, ADR-073 entry added in Integration section and Decision Timeline. Release docs housekeeping: RELEASE_NOTES/GITHUB_1.5.0.md moved from root to docs/releases/; 1.3.3 and 1.3.4 archived. Side note flagged: mqttharebootdetection setting is effectively a no-op post-ADR-073 -- noted in MQTT.md docs, no code change.
<!-- SECTION:FINAL_SUMMARY:END -->
