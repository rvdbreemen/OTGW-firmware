---
id: TASK-899
title: 'feat(webui): single wifi-strength icon — arcs fill/color by signal quality'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-21 22:33'
labels: []
dependencies: []
ordinal: 123000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Replace the two-part header indicator (static grey wifi glyph + separate orange signal bars) with ONE wifi-arcs symbol whose arcs light up progressively and colour by quality tier (poor=red, medium=amber, good=green), like a phone wifi indicator. Ethernet still shows the cable icon; AP fallback shows the wifi glyph fully greyed + 'AP MODE'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Header has a single wifi symbol (dot + concentric arcs); the separate #netSignalBars element is removed
- [ ] #2 Arcs light progressively by quality: more arcs coloured = better reception; colour tier poor/medium/good reuses --status-error/warn/ok
- [ ] #3 AP fallback shows all arcs greyed (none lit) + 'AP MODE'; Ethernet unaffected (cable icon, no wifi)
- [ ] #4 filesystem image rebuilds clean
<!-- AC:END -->
