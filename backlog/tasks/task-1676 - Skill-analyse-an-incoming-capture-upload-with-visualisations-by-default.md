---
id: TASK-1676
title: 'Skill: analyse an incoming capture upload, with visualisations by default'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-06T20:59:00Z'
updated_date: '2026-11-06T21:14:00Z'
labels:
  - tooling
  - skill
dependencies: []
priority: medium
ordinal: 799000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-06: make an analysis skill for when a data capture comes in (Discord, GitHub, local). Visualisations are valuable by default and belong in the skill. Based on the manual analysis of maarten_41762's 24 h heat-pump capture (TASK-1563 notes).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Skill .claude/skills/analyze-capture/SKILL.md describes the workflow: fetch into an empty dir, read summary/gaps/index, run the summary script, interpret against the OT spec, label facts vs estimates, questions for the reporter, Discord only after go
- [x] #2 Bundled script capture_summary.py turns an upload zip/7z or folder into a summary (frame counts, master constants, status segments with run lengths, starts, min on/off, gateway R/A overrides, MQTT commands, broker mismatch) and is verified on the Maarten capture with the same numbers as the manual analysis
- [x] #3 Bundled script capture_plot.py renders the overview chart and run zooms by default, with optional markers and extra CSV series, verified by rendering the Maarten capture and inspecting the PNGs
- [x] #4 Skill appears in the skill list and its description triggers on capture uploads
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Built: .claude/skills/analyze-capture/ with SKILL.md (workflow: fetch into an empty dir, count with the summary script, interpret against the OT spec in time order, facts vs estimates, SAT ground-truth rule, charts by default, report to Robert first, Discord only on request with the posting rules and the webhook route for images, numbers into the backlog), references/capture-format.md (files, source letters, frame decode, status bits), scripts/capture_summary.py and scripts/capture_plot.py (standard library + matplotlib; 7z via 7-Zip).

Evidence on maarten_41762's 24 h capture (7z, 377,702 frames): capture_summary.py reproduces the manual analysis: 5 runs (CH 49.19/49.2/49.21/49.22 min with 119-120 s demand without flame, DHW 39.4 min with 69 s), shortest off 26.4 min, start to start 180.6 min, master constants CS 35 / MaxTSet 35 / room setpoint 20 / MM 100, R id0 1/0 x29970, R id11 x217, HW=0 21:22:26 and HW=1 04:31:01, and flags the broker MISMATCH (typed 192.168.1.101, gateway 192.168.2.101:1883). Runtime 20 s. capture_plot.py rendered capture-overview.png and capture-runs.png with room CSVs, shaded blocked hours, an HW=0 band and 4 markers; inspected both PNGs, fixed a legend overlap and a notes margin. skill-creator quick_validate: 'Skill is valid!' (after removing angle brackets from the description); the skill shows in the session skill list.

Not done: the skill-creator with/without-skill benchmark (subagent runs) was not run; offered to Robert.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC4 done. New project skill analyze-capture with a summary script and a plot script, both verified on the Maarten heat-pump capture (same numbers as the manual analysis, broker mismatch detected, two charts rendered and inspected), validated by skill-creator and listed in the session. Not run: the formal with/without-skill eval benchmark.
<!-- SECTION:FINAL_SUMMARY:END -->
