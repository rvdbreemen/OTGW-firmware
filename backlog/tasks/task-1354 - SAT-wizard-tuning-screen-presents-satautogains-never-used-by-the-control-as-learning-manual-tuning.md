---
id: TASK-1354
title: >-
  SAT wizard: tuning screen presents satautogains (never used by the control) as
  learning / manual tuning
status: Done
assignee:
  - '@claude'
created_date: '2026-10-24T16:29:00Z'
updated_date: '2026-10-24T18:17:00Z'
labels:
  - sat
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 489000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md) finding #67: data/v2.js:5440,5531,5540 present satautogains, which SATpid.ino:120-141 never reads, as 'SAT learns ... adjusts gains' and manual tuning. Misleads users. UI text only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Wizard text no longer claims a learning or gains effect for satautogains (browser test + screenshot)
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT wizard tuning screen no longer claims SAT learns over ~2 weeks or that satautogains tunes anything: no firmware code reads satautogains (settings.sat.fAutoGainsValue; grep), and the PID always computes gains from the heating curve (SATpid.ino:120-141; bAutoGains is never set, default true). Automatic keeps the stored slope; Manual sets the slope and SAT still computes the gains. Evidence: tests/webui/sat-wizard-tuning-text.test.mjs (real page) 11/11 on fix, HEAD 8 failures; screenshots before/after in scratchpad; build SUCCESS. Follow-up idea: bAutoGains / satautogains are dead settings (not removed here).
<!-- SECTION:FINAL_SUMMARY:END -->
