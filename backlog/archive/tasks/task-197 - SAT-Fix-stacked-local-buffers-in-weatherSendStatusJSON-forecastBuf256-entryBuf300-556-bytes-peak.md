---
id: TASK-197
title: >-
  SAT: Fix stacked local buffers in weatherSendStatusJSON (forecastBuf[256] +
  entryBuf[300] = 556 bytes peak)
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:21'
updated_date: '2026-04-09 10:33'
labels:
  - audit-fix
  - sat
  - memory
dependencies: []
priority: high
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 forecastBuf[256] and entryBuf[300] are declared in same scope block, creating 556-byte peak stack allocation
- [x] #2 Refactor to reuse a single buffer or make one static, eliminating the stacked peak
- [x] #3 Verify no off-by-one in new implementation
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate forecastBuf/entryBuf in SATweather.ino (lines 232-254)
2. Apply Option B: reuse single buffer entryBuf[300] for both building the forecast array and the final JSON output
3. Build prefix into entryBuf first, then append array chars directly, eliminating forecastBuf[256]
4. Verify size: prefix 12 chars + max 169 array chars = 181 chars well within 300
5. No re-entrancy concern: function called from HTTP handler only
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Eliminated stacked 556-byte local buffer allocation in weatherSendStatusJSON.

Changes:
- Removed forecastBuf[256] and entryBuf[300] co-existing on the stack simultaneously
- Replaced with single entryBuf[300] that serves both roles: first receives the JSON key prefix via snprintf_P(PSTR(""forecast":[")), then the forecast array is built directly into the remaining space
- Net stack savings: 256 bytes at peak (from 556 down to 308 bytes for this block)
- Logic and JSON output unchanged; size guard thresholds adjusted to match entryBuf bounds
- Uses existing snprintf_P/PSTR pattern established throughout the codebase

Verification:
- Prefix "forecast":[ is 12 chars; max array content ~169 chars (24 temps * 6 + 23 commas + brackets)
- Total max: 181 chars well within entryBuf[300]
- No re-entrancy concern: weatherSendStatusJSON is called from HTTP handler only
- No off-by-one: guard checks pos + len < sizeof(entryBuf) - 2 before appending; closing ] + NUL always fits
<!-- SECTION:FINAL_SUMMARY:END -->
