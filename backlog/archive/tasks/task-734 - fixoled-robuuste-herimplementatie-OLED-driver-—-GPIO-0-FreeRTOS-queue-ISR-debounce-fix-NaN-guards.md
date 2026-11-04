---
id: TASK-734
title: >-
  fix(oled): robuuste herimplementatie OLED driver — GPIO 0, FreeRTOS queue ISR,
  debounce fix, NaN guards
status: Done
assignee: []
created_date: '2026-05-27 20:23'
updated_date: '2026-05-27 22:33'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
OLED.ino bevat 6 gebreken t.o.v. OT-Thing referentie. Plan: ~/.claude/plans/in-het-other-project-is-wise-simon.md. Gebruik team-reviewer (5 dimensies) dan team-implementer (OLED.ino eigendom).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Knop gebruikt GPIO 0 (PIN_BUTTON), niet GPIO 9
- [x] #2 ISR gebruikt FreeRTOS queue (xQueueSendFromISR), geen volatile bool
- [x] #3 Debounce-check vindt VOOR state-mutatie plaats (OT-Thing patroon)
- [x] #4 Alle 10 float-velden hebben isnan()-bewaking
- [x] #5 oled.clear() alleen op paginawisseling, niet elke seconde
- [x] #6 WiFi SSID gecached als char[], geen String in render-loop (ADR-004)
- [x] #7 build.py (firmware + filesystem) slaagt zonder errors/warnings
- [x] #8 evaluate.py --quick toont geen nieuwe failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
OLED driver herschreven: GPIO 0, FreeRTOS queue ISR, debounce fix, NaN guards (fmtFloatOrDash helper), selective clear, SSID cache, 5s cadans.
<!-- SECTION:FINAL_SUMMARY:END -->
