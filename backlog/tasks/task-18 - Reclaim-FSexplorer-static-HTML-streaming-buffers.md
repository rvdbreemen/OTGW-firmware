---
id: TASK-18
title: Reclaim FSexplorer static HTML streaming buffers
status: Done
assignee: []
created_date: '2026-03-18 19:44'
updated_date: '2026-03-18 21:08'
labels:
  - memory performance filesystem
dependencies: []
references:
  - 'src/OTGW-firmware/FSexplorer.ino:123'
  - 'src/OTGW-firmware/FSexplorer.ino:140'
  - 'src/OTGW-firmware/FSexplorer.ino:147'
priority: high
ordinal: 1000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
FSexplorer currently keeps three permanent 512-byte static buffers alive for the lifetime of the firmware: lineBuf[512] and two outBuf[512] instances in the index.html streaming handler. This costs about 1536 bytes of persistent RAM to avoid String allocations. The goal is to preserve the no-String streaming behavior from TASK-7 while reducing permanent RAM usage by reusing a single scratch buffer, sharing workspace across branches, or emitting transformed chunks without duplicate persistent buffers.

Evaluate the option to use the global scratch buffers that are available.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Persistent RAM used by the FSexplorer HTML streaming handler is reduced by at least 1024 bytes
- [x] #2 index.html and graph.js cache-busting behavior remains unchanged
- [x] #3 The handler continues to avoid Arduino String allocations in the hot path
- [x] #4 Chunked transfer behavior remains correct for empty lines and end-of-response handling
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Removed the two static outBuf[512] allocations from the FSexplorer index.html streaming handler. Cache-busted src replacements now stream prefix, version token, hash, and suffix directly using the existing lineBuf[512], preserving String-free chunked output while reclaiming about 1024 bytes of persistent RAM.
<!-- SECTION:NOTES:END -->
