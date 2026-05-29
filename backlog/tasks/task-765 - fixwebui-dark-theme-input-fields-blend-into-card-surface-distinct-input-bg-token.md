---
id: TASK-765
title: >-
  fix(webui): dark-theme input fields blend into card surface (distinct
  --input-bg token)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-29 19:21'
updated_date: '2026-05-29 19:21'
labels:
  - webui
  - darktheme
  - design-system
  - field-report
milestone: 2.0.0
dependencies: []
references:
  - src/OTGW-firmware/data/ds-tokens.css
  - 'src/OTGW-firmware/data/components.css:201'
  - 'src/OTGW-firmware/data/components.css:984'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report from Robert (alpha.97, dark theme): settings input fields render the same colour as the card/row surface in dark mode, so a field is indistinguishable from its background. Root cause: inputs draw their background from var(--bg-surface), which in dark equals the settings row/card surface (#2c2c2e). In light it was fine only by accident (inputs white, rows light-blue).

Fix (design-system tokens, no new hex): add a semantic --input-bg (and --input-bg-changed) token aliased from existing surface scale tokens -- light: var(--bg-surface) (white, unchanged); dark: var(--bg-surface-alt) (#3a3a3c, one step up from the #2c2c2e card so a field reads as a field). Route the base input rule and the .input-normal/.input-changed/.input-readonly state classes (incl. number inputs) through these tokens so every input state is theme-aware (previously .input-normal hardcoded white/black, which also flashed white inputs in dark after a refresh).

Verified by loading the REAL index.html + real index.js + real ds-tokens/components.css under Playwright and rendering the actual settings page in BOTH themes: dark input bg #3a3a3c is now distinct from the #2c2c2e card surface; light unchanged (white input on light-blue row); cards still cap at 520px. This is the faithful test I should have run for TASK-763 from the start.

2.0.0-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Dark theme: settings input background is visibly distinct from the card/row surface (input #3a3a3c vs surface #2c2c2e)
- [x] #2 Light theme unchanged (white input on the light-blue row); no regression
- [x] #3 Input backgrounds use design-system tokens aliased from existing surface scale (no new raw hex); .input-normal/.input-changed/.input-readonly are theme-aware
- [x] #4 Verified in the real index.html (real index.js + CSS) in both themes; settings cards still cap at 520px
- [ ] #5 python build.py green; python evaluate.py --quick no new failures (design-system drift gate)
<!-- AC:END -->
