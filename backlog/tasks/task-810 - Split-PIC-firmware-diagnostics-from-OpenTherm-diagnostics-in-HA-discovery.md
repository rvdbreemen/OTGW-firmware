---
id: TASK-810
title: Split PIC firmware diagnostics from OpenTherm diagnostics in HA discovery
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 06:20'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field feedback: HA discovery currently lumps OpenTherm diagnostics, ESP/firmware stats, and PIC firmware info/settings/controls into one device's single 'diagnostic' entity_category. Split them so PIC-related and ESP/stats-related entities form distinct groupings separate from OpenTherm diagnostics. HA's only native grouping beyond entity_category is separate devices linked via via_device.
<!-- SECTION:DESCRIPTION:END -->
