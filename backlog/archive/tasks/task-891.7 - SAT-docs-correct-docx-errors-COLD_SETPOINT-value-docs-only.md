---
id: TASK-891.7
title: 'SAT docs: correct docx errors + COLD_SETPOINT value (docs-only)'
status: Done
assignee:
  - '@claude'
created_date: '2026-06-20 19:22'
updated_date: '2026-06-20 21:04'
labels: []
milestone: 2.0.0
dependencies: []
references:
  - docs/sat/SAT_thermo_nova_Documentation.docx
parent_task_id: TASK-891
priority: low
ordinal: 114000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT docx has 3 transcription errors vs its own Python source (firmware matches Python). George ruled 'Python always wins'. Correct the docx; docs-only, no firmware change, no version bump. George patches the Python COLD_SETPOINT const separately.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Solar-gain hold corrected 2h -> 600s/10min (Python SOLAR_GAIN_HOLD_SECONDS=600)
- [x] #2 Overshoot margin: section 3.3 '2C' corrected to 3.0 to match section 9.2 and Python (OVERSHOOT_MARGIN_CELSIUS=3.0)
- [x] #3 Remove/clarify the alpha=0.2 low-pass on requested_setpoint (section 8.1): Python has no setpoint low-pass, only the PID derivative time-based filter (pid.py:227-230)
- [x] #4 COLD_SETPOINT value corrected 22 -> per-system (radiators 28.2, underfloor 21) per George 2026-06-20
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Corrected the SAT thermo-nova .docx to match its Python source (George: Python wins): solar-gain hold 2h->10min, overshoot section 3.3 2C->3C, dropped the misleading [alpha=0.2 low-pass] annotation on requested_setpoint, COLD_SETPOINT 22C->per-system 28.2C(radiators)/21C(underfloor). Targeted in-place edits to word/document.xml with per-replacement uniqueness asserts + XML-wellformed verify; backup kept in TEMP. Docs-only, no bump. Committed+pushed dd99437a.
<!-- SECTION:FINAL_SUMMARY:END -->
