---
id: TASK-1604
title: >-
  Outbound fetches off the loop task: weather fetch in a one-shot task, and the
  PIC refresh stops after a failed update check (TASK-1471 R1, R2)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-04T15:53:00Z'
updated_date: '2026-11-04T18:10:00Z'
labels:
  - network
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 745000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-1471 audit (docs/audits/2026-11-04-outbound-connections.md). R1: the SAT weather HTTP GET runs on the loop task (SATweather.ino:343-375); with internet blocked it stalls the loop 5 s or more per attempt (dropped SYN, 5 s connect default) and longer when DNS is blocked, at +300/360/480/720/1200 s after boot. R2: refreshpic() (OTGW-Core.ino ~6936-6946) continues with a GET when checkforupdatepic() returned an empty string after a failed HEAD (latest != version), adding up to ~20 s of blocked loop per click.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 R1: the weather fetch runs outside the loop task (one-shot FreeRTOS task like webhook.ino:336, or the async HTTP pattern) with a connect timeout <= 2 s; the loop never blocks on it (host/source test + bench max_loop_gap_ms with internet blocked equal to baseline)
- [x] #2 R2: refreshpic() returns without a GET when the update check gave no version (host or source test red on old code)
- [ ] #3 Bench .63 with a scoped MikroTik drop rule for outbound internet: no loop-stall line from weather or PIC paths, no reboot, settings unchanged
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-04 (sonnet agent; lead reran in wt-int4): R1 weather fetch on a one-shot worker (8192 B stack, prio 1, app core, via platformTaskCreatePinned + new platformTaskExit shim), IDLE/RUNNING/READY atomic handoff, the loop task applies the result (no SAT state written by the worker), setConnectTimeout(2000), no start at HEAP_CRITICAL; telnet 'w' prints when the result is consumed. R2 refreshpic returns on an empty version. tests/test_weather_worker_handoff.py 10 OK (old: 10 FAIL), stream_tail 4, snowfall 3, settings_float 2 OK, evaluate 0 failed. Tag beta.75; bench pending (TASK-1471 T1 red run on beta.74 running).

2026-11-04 bench beta.76 on .61 (T1: MikroTik forward drop .61 -> internet, 18:14-18:39, rule auto-removed, 'left: 0'): 5 weather attempts (18:19, 18:20, 18:22, 18:26, 18:34), each 'Net: DNS lookup failed' after ~14 s inside the worker task (stack 31732 vs loop 32756); bootcount 42 the whole run (no restart), hd_max_loop_gap_ms 456 constant; reboot_log only the flash lines. Same rule on .63 with beta.74 (16:56-17:21): 5 Exception/panic restarts. Same-board (.63) green run with beta.77 follows after the 20:00 publish. AC1 bench half: max loop gap with internet blocked equal to baseline (456 ms).
<!-- SECTION:NOTES:END -->
