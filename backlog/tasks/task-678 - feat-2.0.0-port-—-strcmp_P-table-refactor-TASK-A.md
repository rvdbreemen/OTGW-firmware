---
id: TASK-678
title: 'feat-2.0.0: port — strcmp_P table refactor (TASK-A)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-23 16:05'
updated_date: '2026-05-23 16:08'
labels: []
dependencies: []
ordinal: 55000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling port of dev TASK-A. Collapse 12 uniform 'else if (strcmp_P(buf, PSTR(...)) == 0) { Debugln(F(...)); reportOTGWEvent_P(PSTR(...), ?, true); }' branches in OTGW-Core.ino (lines 4156-4192) into a single PROGMEM lookup table + loop. Removes ~36 LOC of mechanical duplication; behaviour byte-identical.

Master plan: /root/.claude/plans/evaluate-the-found-issues-misty-frog.md
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 PROGMEM table holds 12 entries: {token, message, severityChar}; suppressDuringStartup is implicit true (matches every existing branch)
- [x] #2 Behaviour byte-identical: each of NG/SE/BV/OR/NS/NF/OE/Thermostat connected/Thermostat disconnected/Low power/Medium power/High power produces same Debugln + reportOTGWEvent_P semantics with same severity char and suppression flag
- [x] #3 python build.py --firmware --target esp8266 exits 0
- [x] #4 python evaluate.py --quick: no new failures (baseline: 60 passed, 0 failed)
- [x] #5 Commit pushed to claude/beta20-improvements-2.0.0-xRpVI
- [ ] #6 PROGMEM table refactor: 12 hand-written else-if branches collapsed into 1 lookup loop + 12-row table; net LOC delta on OTGW-Core.ino is +31 (38 lines of mechanical duplication replaced by named-string declarations + structured table — clarity gain over pure LOC reduction)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Build PROGMEM table: { const char* token, const char* msg, char severity }. 12 entries.
2. Strings stored as static const char tok_NG[] PROGMEM = "NG"; etc — keeps everything in flash.
3. Replace the 12 'else if' branches with a single loop that walks the table and calls Debug+reportOTGWEvent_P.
4. Debugln must show same string; use Debugln + __FlashStringHelper cast on the message PROGMEM pointer.
5. reportOTGWEvent_P takes PGM_P directly — pass msg PROGMEM pointer through pgm_read_ptr.
6. Verify build green (ESP8266), evaluator no new failures.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
TASK-A-2.0.0 implementation complete.

- Replaced 12 else-if branches in OTGW-Core.ino:4156-4192 with kPICStatusTokens lookup table.

- All token + message strings stored in named PROGMEM (named_const-char-arrays before the table).

- handlePICStatusToken() helper walks the table with pgm_read_ptr + strcmp_P + Debugln/__FlashStringHelper cast + reportOTGWEvent_P.

- Build: ESP8266 target builds clean (0.82 MB merged binary).

- Build: ESP32 target build cannot run in sandbox (SSL self-signed in chain to github.com when fetching framework-arduinoespressif32; same as TASK-675).

- Evaluator: 60 passed, 0 failed, health 98.5 (identical to pre-change baseline).

- LOC delta: +31 lines net on OTGW-Core.ino — table is more verbose than per-branch PSTR duplication because named-string declarations are required for table-of-pointers idiom. Clarity gain (single lookup loop vs. 12 mechanical branches) is the actual win.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Collapsed 12 uniform 'else if (strcmp_P(buf, PSTR(...)) == 0) { Debugln + reportOTGWEvent_P }' branches in OTGW-Core.ino into a single PROGMEM kPICStatusTokens table + handlePICStatusToken() lookup loop.

Changes:
- Added 12-entry PROGMEM table (kPICStatusTokens) above processOT(): each row carries token, message, and severity char. Strings live as named PROGMEM declarations so the table can store pointers via pgm_read_ptr.
- Added handlePICStatusToken() helper that walks the table, matches with strcmp_P, and dispatches via Debugln(__FlashStringHelper) + reportOTGWEvent_P with the per-row severity.
- Replaced the 12 hand-written branches at lines 4156-4192 with a single 'else if (handlePICStatusToken(buf))' branch.

Behaviour parity: every original branch produced (a) Debugln of the message text, (b) reportOTGWEvent_P(message, severity, true). Both are preserved row-by-row.

Build (ESP8266): clean, 0.82 MB merged binary.
Build (ESP32): blocked in sandbox by SSL cert issue on github.com (pre-existing — same as TASK-675).
Evaluator: 60/0 failed, health 98.5 (identical to baseline).
<!-- SECTION:FINAL_SUMMARY:END -->
