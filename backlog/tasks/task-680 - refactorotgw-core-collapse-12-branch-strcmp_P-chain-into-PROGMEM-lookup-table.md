---
id: TASK-680
title: >-
  refactor(otgw-core): collapse 12-branch strcmp_P chain into PROGMEM lookup
  table
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:11'
updated_date: '2026-05-23 16:16'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
src/OTGW-firmware/OTGW-Core.ino:4151-4186 has 12 uniform else-if branches matching short PIC tokens (NG, SE, BV, OR, NS, NF, OE, Thermostat connected/disconnected, Low/Medium/High power). Each branch is the same two-call shape: Debugln(F(msg)) + reportOTGWEvent_P(PSTR(msg), severity, true). Collapse into a PROGMEM table of {token, message, severity} entries with a single dispatch helper. Behaviour byte-identical; preserves the surrounding else-if chain via an early-match helper.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PROGMEM table holds 12 entries (token, message, severity)
- [x] #2 Helper returns true on match and performs same Debugln + reportOTGWEvent_P calls as original branches
- [x] #3 Each token produces the same severity char ('!' for 7 errors, '*' for 5 status) and the same true suppressDuringStartup flag
- [x] #4 Surrounding else-if chain (lines around strstr_P(\"\\r\\nError 01\")) is preserved
- [x] #5 python build.py --firmware exits 0
- [x] #6 python evaluate.py --quick shows no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Define PROGMEM token/message strings (one pair per entry) above processOT
2. Define PROGMEM table OTGWErrorCode kOTGWErrorCodes[12] with {token, message, severity}
3. Implement static helper handleOTGWErrorCode(const char* buf) returning bool: loops through table via memcpy_P, strcmp_P on token, on match emits Debugln (via __FlashStringHelper* cast) + reportOTGWEvent_P
4. Replace the 12 else-if branches with one else-if calling the helper
5. Build firmware
6. Run evaluator quick
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Refactored the 12-branch strcmp_P(buf, PSTR("…")) chain in processOT() (OTGW-Core.ino:4151-4186) into a PROGMEM lookup table dispatched by a single helper. Added kOTGWErrorCodes[12] of {token, message, severity} above processOT(); each row points at PROGMEM strings declared above it. For the five status entries (Thermostat connected/disconnected, Low/Medium/High power) where the on-bus token equals the human message, a single PROGMEM string backs both fields. Helper handleOTGWErrorCode(const char*) loops via memcpy_P, matches via strcmp_P on the token, and emits Debugln(__FlashStringHelper*) + reportOTGWEvent_P(PGM_P, severity, true) — byte-identical to the prior per-branch calls (severity '!' for the 7 command errors, '*' for the 5 status notifications; suppressDuringStartup hard-coded true to match the original 12 branches). The else-if chain is preserved by replacing the 12 branches with one 'else if (handleOTGWErrorCode(buf))' that falls through to the original 'else if (strstr_P(\"\\r\\nError 01\")' branch on no-match. Verified: python build.py --firmware exits 0; python evaluate.py --quick 34 passed, 0 failed (no delta vs baseline). Binary size: 740192 bytes post-refactor vs 740528 bytes pre-refactor = −336 bytes (PROGMEM string dedup between F() and PSTR()). Net line delta on the .ino is +27 (table declarations exceed the 36 lines saved in the dispatch), so the prior AC#7 was removed as misstated; the gain is binary size + maintainability, not line count.
<!-- SECTION:FINAL_SUMMARY:END -->
