---
id: TASK-168
title: 'OTGW32-Audit-8B: PROGMEM usage audit in OTDirect.ino'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:23'
updated_date: '2026-04-08 22:32'
labels:
  - audit
  - otgw32
  - phase-8
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
  - docs/adr/ADR-009-progmem-string-literals.md
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scan OTDirect.ino for all string literals and verify they use F() or PSTR() to store them in flash rather than RAM. On ESP32 the F() macro is a no-op (all flash is memory-mapped), but consistent PROGMEM usage keeps the code portable and correct for potential ESP8266 conditional paths. Flag any bare string literals in DebugTf/snprintf calls.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All DebugTln() calls use F() macro for string literals
- [x] #2 All DebugTf()/snprintf() calls use PSTR() for format strings
- [x] #3 No bare C-string literals are used where F()/PSTR() should be used
- [x] #4 snprintf_P is used instead of snprintf for all PSTR format strings
- [x] #5 Any violation results in an audit-fix task
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Grep all DebugTln() calls and check each uses F()
2. Grep all DebugTf() calls and check each uses PSTR()
3. Grep all snprintf calls and verify all are snprintf_P
4. Check for any bare string literals in other contexts
5. Report violations found
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## PROGMEM Audit Results

Searched all DebugTln(), DebugTf() and snprintf calls in OTDirect.ino.

### DebugTln() calls (44 total):
All use F() macro. Zero violations.
Examples: DebugTln(F("OT-direct: ...")); - consistent throughout.

### DebugTf() calls (9 total):
All use PSTR() as format string. Zero violations.
Examples: DebugTf(PSTR("OT-direct: Mode=%d (%s)
"), ...) - consistent throughout.

### snprintf calls:
Zero bare snprintf() calls found. All formatting uses snprintf_P() with PSTR().
Grep for snprintf[^_] returned no matches in this file.

### Other string contexts:
- processOT("BV", 2), processOT("NG", 2) etc: 2-char literal command codes passed as raw strings. These are 3-byte literals (including null). These are NOT debug strings and do not need F()/PSTR() since they are data, not user-visible messages. This is acceptable per project convention (processOT handles its own buffering).
- const char* modeNames[] inside initOTDirect(): local array of char pointers to string literals - these are stack-local, used once in setup, acceptable.
- updateSetting("OTDmode", modeBuf): bare string literal key passed to setting API - consistent with rest of codebase.

### Conclusion:
No PROGMEM violations found in OTDirect.ino.
All debug output uses F() or PSTR() correctly.
All formatting uses snprintf_P throughout.
No audit-fix task required.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed PROGMEM audit of OTDirect.ino. Zero violations found.

Audit scope:
- 44 DebugTln() calls: all correctly use F() macro
- 9 DebugTf() calls: all correctly use PSTR() format strings
- 0 bare snprintf() calls: all formatting uses snprintf_P() with PSTR()

The only bare string literals present are:
- 2-character OT protocol status codes ("BV", "NG", "NF" etc.) passed to processOT() as data, not debug output. This is correct and consistent with the rest of the codebase.
- Short setting keys like "OTDmode" passed to updateSetting() - consistent with codebase convention.

OTDirect.ino is fully PROGMEM-compliant. No audit-fix task required.
<!-- SECTION:FINAL_SUMMARY:END -->
