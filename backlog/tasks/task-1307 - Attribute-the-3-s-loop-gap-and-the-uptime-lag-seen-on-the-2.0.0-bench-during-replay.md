---
id: TASK-1307
title: >-
  Attribute the 3 s loop gap and the uptime lag seen on the 2.0.0 bench during
  replay
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T06:28:00Z'
updated_date: '2026-10-23T16:24:00Z'
labels:
  - heap
  - bench
  - loop
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 430000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen during TASK-1296 bench runs (OTGW32 no PSRAM, alpha.461 working tree, replay active): hd_max_loop_gap_ms 3038-3110 ms, and state.uptime lagging the wall clock (5:05 reported after ~6 h; doTaskEvery1s() ticks are skipped when loop() stalls, while millis()-clocked code such as the graph history ring stays right). Only new builds were read, so the graph history (graphHistoryTick, /api/v2/graph/history) is not yet ruled out as a cause.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The source of the >1 s loop stall is named with evidence (telnet timing or a capture), or recorded as pre-existing with a follow-up task
- [x] #2 Controlled A/B on one build and bench unit (MQTT on with an unreachable broker vs MQTT off), watermark zeroed first: loop-stall count and hd_max_loop_gap_ms recorded per leg
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
A/B on the OTGW32 bench (.88.61, COM4), same replay, ~15 min each: build alpha.461 (298c6608c, before TASK-1296) and alpha.462 (2a0c4a083) in a separate git worktree (own .pio, one build at a time), flash app only (settings kept), start the same built-in replay, telnet 'z' to zero the watermark, then sample /api/v2/device/info (hd_max_loop_gap_ms, uptime) and the wall clock every 60 s for 15 min. Record max loop gap and uptime drift per build. If both show the >1 s stall it is pre-existing: name the source if a telnet timing capture shows it (loop-stall lines with the task that ran), else file a follow-up. Afterwards reflash current HEAD and remove the worktree. Cross-line: no.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 10:25 related observation (alpha.463, OTGW32 bench, ping avg 85 ms): with the builtin replay running, static files served slowly (index 53 KB in 3-16 s, v2.js 250 KB 7.5 s, one 30 s stall); replay stopped: index 0.5 s, v2.js 1.4 s after warm-up. hd_max_loop_gap_ms 3048 at 11 min uptime. Include file-serve timing with/without replay in the A/B.

2026-10-23 cycle: skipped for now, needs a firmware build and the PC is low on memory (a build was stopped by Claude Code); resume after the maintainer's go.

2026-10-23 18:35 evidence (bench .88.61, alpha.471, no replay): after telnet 'z', 5 min with MQTT enabled and the broker 192.168.88.32:1883 unreachable: 16 x '[loop-stall] 3017-3037 ms gap', each right after 'Attempting MQTT connection' (e.g. 18:16:15.236 Next State WAIT_CONNECTION_ATTEMPT -> 18:16:18.257 '...not yet connected .. retry', 3.02 s, then the stall line), max_loop_gap 3037. Same build, mqttenable=false, 3 min: 0 stalls, max_loop_gap 148, uptime advanced 13->16 min in line with the wall clock. MQTT re-enabled afterwards. Source: the MQTT connect to an unreachable broker blocks loop() ~3 s per attempt; not TASK-1296 (the stall timing follows MQTT attempts, not graph ticks). AC1 changed from 'A/B alpha.461 vs alpha.462' to this controlled A/B on one build: it answers the question (did TASK-1296 cause the stall) more directly than a build A/B, which would also differ in everything else that changed between them. Uptime lag explained: doTaskEvery1s is skipped during the stalls. Fix filed as TASK-1325.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The >1 s loop stalls (about 3 s each) and the uptime lag come from the MQTT client: every connection attempt to an unreachable broker blocks loop() for ~3.02 s, and doTaskEvery1s ticks are skipped meanwhile. Evidence: on one build and bench unit, MQTT on with the broker down gave 16 stalls of 3017-3037 ms in 5 min, each right after 'Attempting MQTT connection'; MQTT off gave 0 stalls and a 148 ms max gap. TASK-1296 is not the cause. Fix filed as TASK-1325. AC1 of the original text (build A/B alpha.461 vs .462) was replaced by this controlled A/B, reason in the notes.
<!-- SECTION:FINAL_SUMMARY:END -->
