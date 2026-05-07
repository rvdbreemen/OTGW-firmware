---
id: TASK-575
title: 'docs: update documentation for changes since v1.4.1'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-07 22:47'
updated_date: '2026-05-07 22:52'
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
- [x] #1 API docs updated: MQTT.md reflects sibling-suffix shape, worldview semantics, friendly-name changes, diagnostic topics, dropped /gateway sub-topic
- [x] #2 API docs updated: openapi.yaml and README.md reflect new /api/v2/debug endpoint and other endpoint changes
- [x] #3 API docs updated: WEBSOCKET_FLOW.md and WEBSOCKET_QUICK_REFERENCE.md reflect reload-storm mitigation
- [x] #4 Guides updated: FLASH_GUIDE.md covers no-Python flash scripts; BUILD.md covers build wrappers
- [x] #5 ADR cross-references verified: docs/adr/README.md lists ADR-065 through ADR-072
- [x] #6 Cleanup phase complete: old releases archived, misplaced root files moved
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 MQTT.md: 8 sections updated — topic shape, worldview semantics, friendly names, dropped /gateway, diagnostic discovery, republish threshold, discovery sibling-suffix, base suppression removed.\nAC3 WebSocket docs: WEBSOCKET_FLOW.md + QUICK_REFERENCE updated — 250ms reconnect debounce, pagehide shutdown, server burst diagnostics documented.\nAC5 ADR README: all 8 new ADRs (065-072) added with correct statuses and supersession chain.

AC2 openapi.yaml + README.md: added GET /api/v2/debug (auth-gated diagnostic dump) and POST /api/v2/mqtt/republish; legacyport25238enabled added to settings docs.

AC4 FLASH_GUIDE.md + BUILD.md: flash guide updated with no-Python scripts as preferred method, bootloop recovery alternatives, After Flashing section added. BUILD.md had build.sh/bat already documented; added python build.py fallback note.

MQTT_LWT.md: reconnect diagram extended, new 'Reconnect Republish Behaviour' section (5-min threshold, 3 cases), Related Code updated. WIFI_RECOVERY_TRIPLE_RESET.md: untouched — procedure unchanged.

BREAKING_CHANGES.md: new v1.5.0 section prepended with 3 breaking changes: sibling-suffix shape with mosquitto cleanup cmds, /gateway removal, HA friendly name Title Case (no automation breakage, unique_id unchanged).

AC6 Cleanup: 12 releases archived to docs/releases/archive/, docs/archive/ aangemaakt met daily-issue-report.md en upgrade-from-0.x.md. Root bevindt alleen 1.5.0-beta bestanden.

MQTT-message-id-echo-audit.md: geen wijzigingen nodig. Document was al up-to-date: 10 bSlaveEchoesValue=false entries kloppen exact met OTGW-Core.h. TASK-561 en TASK-571 waren al verwerkt in hetzelfde commit als de code.
<!-- SECTION:NOTES:END -->
