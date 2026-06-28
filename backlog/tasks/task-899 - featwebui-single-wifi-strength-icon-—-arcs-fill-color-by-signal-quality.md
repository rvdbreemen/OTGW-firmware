---
id: TASK-899
title: 'feat(webui): single wifi-strength icon — arcs fill/color by signal quality'
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-21 22:33'
updated_date: '2026-06-28 13:04'
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
- [x] #1 Header has a single wifi symbol (dot + concentric arcs); the separate #netSignalBars element is removed
- [x] #2 Arcs light progressively by quality: more arcs coloured = better reception; colour tier poor/medium/good reuses --status-error/warn/ok
- [x] #3 AP fallback shows all arcs greyed (none lit) + 'AP MODE'; Ethernet unaffected (cable icon, no wifi)
- [x] #4 filesystem image rebuilds clean
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Code already implemented + committed: beb6504f (single wifi-strength icon, arcs fill/colour by quality) + 38e2f77d (drive arcs by document order, drop dead positional classes). index.html:44-50 (3 .wifi-seg paths), index.js:416-434 (inside-out lighting by level, tier good/medium/poor/none, AP greyed, Ethernet hides wifi), components.css ~1868 (.wifi-seg tiers, replaces former .net-signal-bars). Ships in the LittleFS image. Remaining: visual/a11y eyeball on a real device (field, soft gate).
<!-- SECTION:NOTES:END -->
