---
id: TASK-1281
title: 'Web UI: optional help text with an info icon for every setting'
status: To Do
assignee: []
created_date: '2026-10-21 09:32'
updated_date: '2026-10-21 09:33'
labels:
  - ui
  - docs
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 410000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feature request gruijter 2026-10-21 (#alpha-testing). Most settings show only a short label (examples: MQTT 'Separate thermostat/boiler topics', 'Legacy OT topic layout', 'Publish on change'; OT-Direct 'Enable slave (thermostat) passthrough', 'Fail-safe on OT bus loss'; SAT 'Summer simmer', 'Flame-off offset', 'Modulation suppression', 'DHW enabled (standalone)' vs 'DHW enable (HW= master)'). Proposal: optional help field in SET_META (v2.js) next to label/hint, hint stays for units; render an info icon after the label, tooltip on hover/focus, popover on tap; 1-2 sentences per setting (what, when to change, default/side effects), sourced from the manual; move the SAT Indoor temperature explanation into its popup. Mind the LittleFS budget (768K, ADR-191) for the added text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 SET_META supports a help field and the settings page renders an accessible info icon with tooltip (hover/focus) and popover (tap)
- [ ] #2 Help text exists for every curated setting, sourced from the manual, checked by a test that lists settings without help
- [ ] #3 The SAT Indoor temperature inline block moves into its popup
- [ ] #4 LittleFS image still fits with margin (size before/after recorded); screenshots desktop and mobile
<!-- AC:END -->
