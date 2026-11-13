---
id: TASK-1910
title: >-
  device/info sometimes sends one extra closing brace under a connection storm
  (09 bench, beta.226 on .61)
status: To Do
assignee: []
created_date: '2026-11-13T18:55:00Z'
labels:
  - bug
  - rest
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 976000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
09: during a 6-thread connection storm, GET /api/v2/device/info returned a complete valid object followed by one extra closing brace (2x open, 3x close), 10 times in the run, 0 in 300 calm calls (bodies in 09's scratchpad b226/reboot2/badjson1..9.bin). psram_free is a constant 0 on .61, so the live ESP.getFreePsram() read in the multi-pass closure (restAPI.ino:3446) is not the cause. A strict JSON client (HA, web UI) rejects such a body. Likely in restSendChunked / the multi-pass closure output path; near TASK-1899 (same handler).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce on a host test of the chunked multi-pass writer or on the bench with the storm script
- [ ] #2 Root cause with file:line; fix makes every body valid JSON under the storm (0 of N bad), red on the old code
<!-- AC:END -->
