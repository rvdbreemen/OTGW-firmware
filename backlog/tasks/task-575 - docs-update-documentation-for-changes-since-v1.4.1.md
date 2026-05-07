---
id: TASK-575
title: 'docs: update documentation for changes since v1.4.1'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 22:47'
labels:
  - docs
  - update-docs
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Full-scope documentation update for firmware changes v1.4.1..HEAD. Triggered by /update-docs.\n\nSubsystems changed: MQTT (sibling-suffix topic shape ADR-070/071, worldview semantics ADR-069, friendly names TASK-572/573, ADR-066 Write-Ack gate fix TASK-561, TSet bSlaveEchoesValue fix TASK-571, drop /gateway TASK-538, diagnostic HA discovery TASK-540), REST API (new /api/v2/debug endpoint TASK-536), WebSocket (reload-storm mitigation), Network (WiFi TCP-listener re-bind fix), Web UI (index.html/js/css changed), Build/QA (no-Python flash scripts, evaluate.py), ADRs ADR-065 through ADR-072.\n\nSince 6+ subsystems changed, all docs are treated as affected per workflow policy.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 API docs updated: MQTT.md reflects sibling-suffix shape, worldview semantics, friendly-name changes, diagnostic topics, dropped /gateway sub-topic
- [ ] #2 API docs updated: openapi.yaml and README.md reflect new /api/v2/debug endpoint and other endpoint changes
- [ ] #3 API docs updated: WEBSOCKET_FLOW.md and WEBSOCKET_QUICK_REFERENCE.md reflect reload-storm mitigation
- [ ] #4 Guides updated: FLASH_GUIDE.md covers no-Python flash scripts; BUILD.md covers build wrappers
- [ ] #5 ADR cross-references verified: docs/adr/README.md lists ADR-065 through ADR-072
- [ ] #6 Cleanup phase complete: old releases archived, misplaced root files moved
<!-- AC:END -->
