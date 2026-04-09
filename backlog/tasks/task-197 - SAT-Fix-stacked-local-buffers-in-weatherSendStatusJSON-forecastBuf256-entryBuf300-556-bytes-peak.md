---
id: TASK-197
title: >-
  SAT: Fix stacked local buffers in weatherSendStatusJSON (forecastBuf[256] +
  entryBuf[300] = 556 bytes peak)
status: To Do
assignee: []
created_date: '2026-04-09 05:21'
labels:
  - audit-fix
  - sat
  - memory
dependencies: []
priority: high
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 forecastBuf[256] and entryBuf[300] are declared in same scope block, creating 556-byte peak stack allocation
- [ ] #2 Refactor to reuse a single buffer or make one static, eliminating the stacked peak
- [ ] #3 Verify no off-by-one in new implementation
<!-- AC:END -->
