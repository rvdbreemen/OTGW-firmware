---
id: TASK-1243
title: settings.ini can hold 'nan' for a float setting (invalid JSON)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-17 20:54'
updated_date: '2026-10-17 22:07'
labels:
  - bug
  - settings
dependencies: []
ordinal: 381000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by reading code during TASK-1242, not reproduced. updateSetting parses floats with strtof and clamps with constrain(); constrain(NaN, lo, hi) returns NaN because both comparisons are false. writeJsonFloatKV then writes dtostrf(NaN) = 'nan', which is not JSON. A REST POST of the string 'nan' to a float setting is the likely trigger.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Bench: POST value 'nan' to a float setting on the old firmware, show the stored value and the settings.ini line
- [ ] #2 Fix rejects or clamps non-finite floats; same POST on the fix leaves a finite value and valid JSON
<!-- AC:END -->
