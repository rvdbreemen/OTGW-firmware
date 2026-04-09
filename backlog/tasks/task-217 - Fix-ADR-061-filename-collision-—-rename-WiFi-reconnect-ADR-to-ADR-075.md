---
id: TASK-217
title: Fix ADR-061 filename collision — rename WiFi reconnect ADR to ADR-075
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:26'
updated_date: '2026-04-09 05:44'
labels:
  - docs
  - adr
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Two ADR files share the number ADR-061: one for platform abstraction and one for WiFi reconnect timeout tuning. The WiFi reconnect ADR should be renamed to ADR-075 to resolve the collision.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Rename the WiFi reconnect timeout ADR file to ADR-075-wifi-reconnect-timeout.md
- [x] #2 Update any internal references from ADR-061 to ADR-075 for the WiFi reconnect topic
- [x] #3 Verify no other ADR number collisions exist
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Renamed ADR-061-wifi-reconnect-timeout-tuning.md to ADR-075-wifi-reconnect-timeout-tuning.md. Updated internal header from ADR-061 to ADR-075. Updated ADR-047 Superseded reference from ADR-061 to ADR-075. Updated ADR-074 to mark collision as resolved and reference ADR-075. No other ADR number collisions found.
<!-- SECTION:FINAL_SUMMARY:END -->
