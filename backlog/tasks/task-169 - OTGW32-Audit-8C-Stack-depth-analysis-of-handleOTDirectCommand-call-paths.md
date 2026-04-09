---
id: TASK-169
title: 'OTGW32-Audit-8C: Stack depth analysis of handleOTDirectCommand() call paths'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:23'
updated_date: '2026-04-08 22:33'
labels:
  - audit
  - otgw32
  - phase-8
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/OTDirect.ino
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Analyze the deepest call stack path reachable from handleOTDirectCommand() and the OT master send loop. On ESP32-S3 the default stack is much larger than ESP8266, but deep nesting with local buffers can still cause issues. Document the worst-case stack depth and verify that local buffers (rspBuf[16], prBuf[48], etc.) do not collectively exceed safe limits when nested.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Deepest call path from handleOTDirectCommand() is identified and documented
- [x] #2 Total stack usage of that path (all local variables summed) is calculated
- [x] #3 Local buffers rspBuf[16] and prBuf[48] are accounted for
- [x] #4 Stack usage is within ESP32-S3 task stack limits
- [x] #5 If stack depth is a concern an audit-fix task is created
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Identify local variables in handleOTDirectCommand() itself
2. Trace deepest callee chain: synthesizeResponse -> processOT, enqueueWriteCommand -> setOverride/updateWriteCache
3. Look at emitSummaryLine static local buffer (static, not stack)
4. Sum stack frame sizes for worst-case path
5. Compare to ESP32-S3 default task stack (8KB)
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Stack Depth Analysis

### handleOTDirectCommand() local variables:
- char cmd0, cmd1: 2 bytes (chars)
- const char* value: 4 bytes (pointer)
- char rspBuf[16]: 16 bytes
- Various branch-local: int hh, mm, dow (3x4=12 bytes), unsigned int msgId, dataVal (2x4=8), etc.
- PR= branch: char prBuf[48]: 48 bytes, char modeChar: 1 byte, bool csActive/ttActive: 2 bytes, uint16_t csVal/ttVal: 4 bytes, char reg: 1 byte
- TP= branch: unsigned int tpMsgId, tpIndex, tpValue (3x4=12), bool isWrite: 1, char* colonPos/afterColon/eqPos: 3x4=12, OpenThermMessageType: 4, unsigned long tpFrame: 4
- RS= branch: char c0,c1,c2: 3 bytes, uint8_t targetMsgId: 1, unsigned long frame: 4
- Note: branches are mutually exclusive - worst case is the PR= branch with prBuf[48] PLUS rspBuf[16]
- handleOTDirectCommand worst-case stack frame: ~150 bytes conservatively

### Call chains from handleOTDirectCommand():

**Path 1 (deepest): CS=/TT= etc. commands:**
handleOTDirectCommand() → enqueueWriteCommand() → setOverride() [inline loop]
                       → updateWriteCache() [inline loop]
                       → synthesizeResponse(cmd, value) → synthesizeResponse(c0,c1,value)
                         → snprintf_P [no alloc]
                         → processOT(buf, len) [uses only static locals]
enqueueWriteCommand() locals: unsigned long frame: 4 bytes = ~20 bytes total frame
synthesizeResponse(c0,c1,value) locals: char buf[48]: 48 bytes
processOT() locals: time_t now (8 bytes on ESP32), all others are static (not stack)

**Path 2: PR= branch (handleOTDirectCommand with prBuf[48]):**
handleOTDirectCommand() → processOT()
Stack: rspBuf[16] + prBuf[48] + branch vars + frame overhead = ~150 bytes

**Deepest combined path:**
handleOTDirectCommand() [~150 bytes] →
  enqueueWriteCommand() [~30 bytes] →
    synthesizeResponse() char buf[48] [~70 bytes] →
      processOT() [time_t now + static locals = ~20 bytes on stack]

Worst-case total stack depth: ~150 + 30 + 70 + 20 = ~270 bytes

### emitSummaryLine() analysis:
char line[420] is declared STATIC inside emitSummaryLine(). This means it lives in DRAM BSS, NOT on the stack. Stack cost = 0 for that buffer.
local variables: int pos, lambdas (captured refs to pos/line = stack refs), char tmp[12] in lambda = 12 bytes
emitSummaryLine() stack frame: ~50 bytes

### ESP32-S3 stack limits:
- Default Arduino app_main task stack: 8,192 bytes (8KB)
- ESP-IDF default stack for Arduino loop task: 8KB
- loopOTDirect() is called from doBackgroundTasks() which is called from the main loop task
- Worst case ~270 bytes is trivially safe for 8KB stack

### Comparison to ESP8266 (informational):
- On ESP8266 with 4KB CONT stack, 270 bytes is still well within limits
- Note: OTDirect code does NOT run on ESP8266 (HAS_DIRECT_OT=0), so ESP8266 stack is not relevant
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Completed stack depth analysis for handleOTDirectCommand() and related call paths.

Deepest call chain identified:
handleOTDirectCommand() → enqueueWriteCommand() → synthesizeResponse(c0,c1,val) → processOT()

Stack frame breakdown for worst-case path:
- handleOTDirectCommand(): rspBuf[16] + prBuf[48] (PR= branch) + branch locals = ~150 bytes
- enqueueWriteCommand(): frame + unsigned long + label ptr = ~30 bytes
- synthesizeResponse(): char buf[48] + locals = ~70 bytes
- processOT(): time_t now + passed-through params = ~20 bytes (all other locals are static)
- Worst-case total: ~270 bytes

Key finding: emitSummaryLine() char line[420] is declared STATIC, so it contributes 0 bytes to stack depth (lives in DRAM BSS instead).

ESP32-S3 context: Default Arduino loop task stack is 8KB. 270 bytes worst-case is <3.4% of available stack. No risk.

No audit-fix task required.
<!-- SECTION:FINAL_SUMMARY:END -->
