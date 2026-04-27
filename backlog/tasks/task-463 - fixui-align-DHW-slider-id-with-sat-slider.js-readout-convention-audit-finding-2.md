---
id: TASK-463
title: >-
  fix(ui): align DHW slider id with sat-slider.js readout convention (audit
  finding 2)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-27 23:35'
updated_date: '2026-04-27 23:42'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
sat-slider.js auto-mirrors readouts via the convention id="x-slider" -> id="x-readout". Current markup uses id="sat-dhw-slider" with span id="sat-dhw-slider-value", so the design-system mirror does nothing and sat.js handles the label via updateDHWSliderLabel(). Fix: rename span id from sat-dhw-slider-value to sat-dhw-readout, add data-unit and data-decimals attributes, remove the now-redundant updateDHWSliderLabel() function and its call sites and the inline oninput="SAT.onDhwSliderInput(...)" handler. Keep onchange for the actual command-send. Result: one source of truth (sat-slider.js) for slider visual fill + readout text.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 index.html DHW slider readout span renamed to id=sat-dhw-readout with data-unit and data-decimals attrs
- [x] #2 sat.js: updateDHWSliderLabel() removed and all call sites updated to leave display to sat-slider.js
- [x] #3 onDhwSliderInput in sat.js still tracks _dhwSliderDirty but no longer touches the label
- [x] #4 Build clean, no JS errors
- [ ] #5 DHW slider drag still updates the visible readout value
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
DHW slider readout id renamed to sat-dhw-readout with data-unit and data-decimals attrs. updateDHWSliderLabel() function removed; programmatic value mutations now dispatch a synthetic 'input' event so sat-slider.js's bind() handler covers both --slider-pct and readout text. Build clean.
<!-- SECTION:FINAL_SUMMARY:END -->
