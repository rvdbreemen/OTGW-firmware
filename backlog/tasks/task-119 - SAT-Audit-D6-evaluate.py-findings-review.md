---
id: TASK-119
title: 'SAT Audit D6: evaluate.py findings review'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:54'
updated_date: '2026-04-09 05:26'
labels:
  - audit
  - sat
  - phase-4
milestone: m-0
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Run evaluate.py on SAT-related files and review all findings. Check PROGMEM patterns, unsafe patterns and code quality flags.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 evaluate.py run on SATcontrol.ino and related files
- [x] #2 All findings documented
- [x] #3 Follow-up fix tasks created with label 'audit-fix' for each finding
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Run python evaluate.py (full) and capture output\n2. Categorize all findings\n3. Identify which are SAT-related vs pre-existing\n4. Create fix tasks for non-trivial issues
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## evaluate.py Full Findings Review

Run: python evaluate.py (full, not quick)
Score: 86.7% (32 PASS, 5 WARN, 1 FAIL, 7 INFO) -- 45 checks total

### FAIL (1)
- PROGMEM: 15 violations found.
  SAT-specific: 3 bare strcmp() calls in satHandleControlMode() (SATcontrol.ino:1046-1050)
  Non-SAT: 12 in FSexplorer.ino, jsonStuff.ino, MQTTstuff.ino, OTGW-Core.ino, Debug.h, platform_esp32.h
  Fix task created: TASK-205 (SAT violations only)

### WARN (5)
1. String Class Usage: 29 String usages found.
   SAT-specific: 6 (3 in SATble.ino ESP32-only, 1 in SATweather.ino ADR-004 exception, 1 comment only).
   Fix task created: TASK-202 (BLE String heap fragmentation, ESP32 only)
   
2. Stack: 2 non-static local char buffers > 256 bytes found.
   These are: entryBuf[300] in SATweather.ino:246, buf[320] in OTGW-Core.ino:4404
   SAT-specific: entryBuf[300] in weatherSendStatusJSON().
   Fix task created: TASK-197 (stacked buffers)

3. BUILD.md: not found -- pre-existing, not SAT-related
4. FLASH_GUIDE.md: not found -- pre-existing, not SAT-related
5. Git Working Tree: 222 uncommitted changes -- audit branch state, expected

### Summary of SAT-specific findings
- 3 PROGMEM violations: TASK-205
- String heap fragmentation (ESP32 BLE): TASK-202
- Stacked local buffers: TASK-197

All pre-existing non-SAT failures are outside scope of this audit.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
evaluate.py full run reviewed.

Score: 86.7% health (was the same before SAT code addition -- SAT did not regress the score).

SAT-specific findings and their fix tasks:
- 3 PROGMEM violations in satHandleControlMode(): TASK-205
- String heap fragmentation in ESP32 BLE callback: TASK-202
- Stacked local buffers in weatherSendStatusJSON(): TASK-197

All other warnings (BUILD.md missing, FLASH_GUIDE.md missing, git working tree dirty) are pre-existing and unrelated to SAT code.
<!-- SECTION:FINAL_SUMMARY:END -->
