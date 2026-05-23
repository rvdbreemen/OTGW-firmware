---
id: TASK-680
title: >-
  refactor(otgw-core): collapse 12-branch strcmp_P chain into PROGMEM lookup
  table
status: In Progress
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
- [ ] #1 PROGMEM table holds 12 entries (token, message, severity)
- [ ] #2 Helper returns true on match and performs same Debugln + reportOTGWEvent_P calls as original branches
- [ ] #3 Each token produces the same severity char ('!' for 7 errors, '*' for 5 status) and the same true suppressDuringStartup flag
- [ ] #4 Surrounding else-if chain (lines around strstr_P(\"\\r\\nError 01\")) is preserved
- [ ] #5 python build.py --firmware exits 0
- [ ] #6 python evaluate.py --quick shows no new failures
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
