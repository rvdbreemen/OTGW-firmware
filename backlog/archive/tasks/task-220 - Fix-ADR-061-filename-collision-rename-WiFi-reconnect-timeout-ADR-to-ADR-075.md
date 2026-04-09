---
id: TASK-220
title: 'Fix ADR-061 filename collision: rename WiFi reconnect timeout ADR to ADR-075'
status: To Do
assignee: []
created_date: '2026-04-09 05:27'
labels:
  - audit-fix
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two files share ADR number 061: ADR-061-unified-esp8266-esp32-platform-abstraction.md (platform abstraction) and ADR-061-wifi-reconnect-timeout-tuning.md (WiFi reconnect tuning). The WiFi reconnect ADR was assigned the wrong number. It should be renamed to ADR-075-wifi-reconnect-timeout-tuning.md and its internal references updated. This was identified during the SAT audit phase (ADR-074).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Rename ADR-061-wifi-reconnect-timeout-tuning.md to ADR-075-wifi-reconnect-timeout-tuning.md
- [ ] #2 Update internal ADR number reference in the file header
- [ ] #3 Update any cross-references to ADR-061-wifi-reconnect in other ADR files
<!-- AC:END -->
