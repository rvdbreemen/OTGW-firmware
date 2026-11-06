---
id: TASK-1669
title: Free heap drifts ~500 B/h on the Classic+S3 during the beta.95 24 h soak
status: To Do
assignee: []
created_date: '2026-11-05T23:33:00Z'
updated_date: '2026-11-06T00:43:00Z'
labels:
  - heap
  - soak
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 792000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1544 soak on .63 (Classic + S3 Mini, PSRAM, MQTT connected, beta.95+1fbd5cf, boot 148): freeheap 54,176 B at 19:13 -> 51,572 B at 00:28 (2026-11-05/06), a steady ~500 B/h decline; maxfreeblock flat at 31,732, hd_min_free_heap 20,760 -> 19,736. No restart. At this rate ~12 KB/day: a slow leak or a growing structure (graph history ring is in PSRAM there, so not that). Sampler log: lead scratchpad soak24-b95/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Full 24 h curve evaluated (linear vs levelling off) when the soak ends ~2026-11-06 15:00
- [ ] #2 If linear: the growing allocation identified (heap tracing or per-consumer counters) and fixed, red/green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 01:42: soak sampler on .63 (beta.95, boot=148) was stopped by Claude Code's low-memory reaper after 10.5 h uptime; not restarted (needs Robert's ask). Data so far (scratchpad soak24-b95/samples.log): heap 54660 at 17:58 (up 3:01) -> 51116 at 01:28 (up 10:31) = -3544 B in 7.5 h, about 470 B/h, still linear; minfree 20760 -> 19736; maxblk 31732 flat; bootcount 148 unchanged; mqtt connected throughout. The 15:00 evaluation needs one manual device/info + crashlog + reboot_log read.
<!-- SECTION:NOTES:END -->
