---
id: TASK-313
title: '[SEC-L1] Verify flash_esp.py release assets against published SHA256SUMS'
status: To Do
assignee: []
created_date: '2026-04-18 19:23'
labels:
  - security
  - review-2026-04-18
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
flash_esp.py:402 fetches .bin assets from GitHub releases and writes directly to flash, no checksum verification. A compromised CI secret can silently alter a release binary.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Release workflow publishes a SHA256SUMS asset alongside binaries
- [ ] #2 flash_esp.py downloads SHA256SUMS and verifies each artifact before esptool write
- [ ] #3 Mismatch aborts flashing and deletes the local copy
<!-- AC:END -->
