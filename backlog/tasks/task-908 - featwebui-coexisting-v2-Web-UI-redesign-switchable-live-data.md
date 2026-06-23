---
id: TASK-908
title: 'feat(webui): coexisting v2 Web UI redesign (switchable, live data)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-23 22:31'
updated_date: '2026-06-23 22:32'
labels: []
milestone: 2.0.0
dependencies: []
ordinal: 124000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Implement the full multi-page Web UI redesign from the claude.ai design import (Home A/B/C, Monitor, Settings, connectivity strip) shipped ALONGSIDE the existing UI, switchable via a device-wide firmware setting (settings.ui.bUseV2). New UI reuses the existing REST/WS data contract; tokens fold into ds-tokens.css (ADR-091 safe). Design spec: docs/superpowers/specs/2026-06-24-webui-v2-coexisting-redesign-design.md. Epic parent for the phased sub-tasks P0..P5.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Design spec approved by maintainer
- [ ] #2 P0 foundation (tokens + bundle skeleton + device-wide toggle) landed
- [ ] #3 P1 Home A/B/C wired to live data
- [ ] #4 P2 Monitor (log/stats/support/graph/connection) wired
- [ ] #5 P3 Settings + BLE roster wired
- [ ] #6 P4 connectivity strip + detail map wired
- [ ] #7 P5 a11y + field validation + default-UI cutover decision
- [ ] #8 All builds (esp32, esp32-classic) green + evaluator green incl ADR-091 drift gate
<!-- AC:END -->
