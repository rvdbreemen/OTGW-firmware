---
id: TASK-116
title: 'SAT Audit D3: PROGMEM compliance check'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 21:53'
updated_date: '2026-04-09 05:23'
labels:
  - audit
  - sat
  - phase-4
  - memory
  - progmem
milestone: m-0
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Check all string literals in SAT-related code for PROGMEM compliance. All strings must reside in flash via F(), PSTR() or PROGMEM declaration. Run evaluate.py and review all findings.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All string literals in SATcontrol.ino and related files audited
- [x] #2 Strings without PROGMEM/F()/PSTR() identified
- [x] #3 evaluate.py run and output reviewed
- [x] #4 Follow-up fix tasks created with label 'audit-fix' for non-compliant strings
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Run python evaluate.py to get violation list\n2. Filter to SAT-specific violations\n3. Check all 15 violations and attribute to files\n4. Create fix tasks for SAT violations
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## PROGMEM Compliance Check Results

evaluate.py reports 15 violations total. Full list:

### SAT-specific violations (3 in SATcontrol.ino)
1. SATcontrol.ino:1046: `strcmp(value, "1")` -- bare strcmp with string literal instead of strcmp_P(value, PSTR("1"))
2. SATcontrol.ino:1048: `strcmp(value, "2")` -- same issue
3. SATcontrol.ino:1050: `strcmp(value, "0")` -- same issue
   Function: satHandleControlMode()
   Risk: Low -- "1"/"2"/"0" are single-char strings in RAM (the literal is baked into the binary anyway), but technically violates the project rule.

### Non-SAT violations (12 in other files)
4. FSexplorer.ino:104, 190, 203: strcmp with string literals
5. jsonStuff.ino:35: snprintf (not _P) for hex encoding
6. MQTTstuff.ino:1416, 1422, 1461: snprintf without _P
7. OTGW-Core.ino:4608, 4691: strcmp with "unknown"
8. Debug.h:95: snprintf in debug helper
9. platform_esp32.h:181, 191: snprintf in ESP32 preferences shim

### String class usage (evaluate.py counts 29 total)
In SAT files: 6 String usages -- all in SATble.ino (ESP32-only) or SATweather.ino (one-off fetch).
None in hot paths on ESP8266.

### Result
Only 3 SAT-specific violations, all in satHandleControlMode(). Low RAM impact (single char strings).
A fix task is warranted for correctness, but not a crash risk.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
PROGMEM compliance audit of SAT files complete.

Results:
- evaluate.py reports 15 total violations across the codebase; 3 are in SAT code.
- SAT violations: 3 bare strcmp() calls in satHandleControlMode() (SATcontrol.ino:1046-1050) comparing single-digit string literals "0"/"1"/"2" without PSTR().
- All DebugTln/DebugTf/snprintf_P usage in SAT files is correct (uses F() and PSTR() consistently).
- No snprintf (non-P) calls found in SAT files.
- String class usage in SAT files is either ESP32-only (SATble.ino) or explicit ADR-004 exception (SATweather.ino one-off fetch).

Created TASK-205 to fix the 3 bare strcmp violations in satHandleControlMode().
<!-- SECTION:FINAL_SUMMARY:END -->
