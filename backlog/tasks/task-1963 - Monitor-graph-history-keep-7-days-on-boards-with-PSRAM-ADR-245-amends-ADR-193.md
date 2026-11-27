---
id: TASK-1963
title: >-
  Monitor graph history: keep 7 days on boards with PSRAM (ADR-245 amends
  ADR-193)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T09:22:00Z'
updated_date: '2026-11-27T05:48:00Z'
labels:
  - graph
  - webui
  - psram
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 1004000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert's decision A (DM 1571454228763779093, item 1, reply to question 1 of the bot list 1571452589231439943: 'A) keep 7 days on PSRAM boards instead of 24 h (~70 KB PSRAM)'; option B, a flash copy every 15 min, was not chosen). On a board with PSRAM the device graph history (graphHistoryStuff, ADR-193) grows from 24 h to 7 days: GRAPH_HIST_LARGE_SLOTS 726 to about 5046 slots of 120 s (about 70 KB PSRAM at 14 B per slot), /api/v2/graph/history serves a window up to 7 days, the Monitor graph gets a 7 d window chip, and the CSV export covers the window shown. Boards without PSRAM keep their current ring unchanged. The ring stays in RAM only: a reboot still empties it. Related: ADR-193, TASK-1306 (response cost of the history on async_tcp), TASK-1776.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 ADR-245 (amends ADR-193) records Robert's decision with the DM id, the slot count, the PSRAM cost and the served window; created and gated through adr-kit
- [x] #2 Host test on the real graphHistoryStuff code: on a PSRAM board the ring holds 7 days at 120 s per slot and the history endpoint serves up to 7 days; without PSRAM the slot count and window are unchanged; red on the current code, green on the fix
- [x] #3 Monitor > Graph offers a 7 d window (chip), the CSV export covers the window shown; web UI test on the real v2.js
- [x] #4 Bench on .63 (PSRAM): free PSRAM and internal heap before and after (the ring cost matches the estimate), /api/v2/graph/history with the full window answers without a web connection refusal, and the graph shows the 7 d window
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Robert's A (DM 1571454228763779093) and A for the serving question + ADR-245 accept (DM 1571457707725168736 on 1571457301280202873). Firmware: GRAPH_HIST_LARGE_SLOTS 726 -> 5046 (7 d at 120 s, 70644 B PSRAM); graphHistMakeSnapshot(windowS) with default 24 h, cap 7 d, at most GRAPH_HIST_MAX_EMIT 1008 slots per response, longer windows thinned to groups aligned to the newest slot (temps/mod newest non-empty, flags OR); sendGraphHistory reads ?hours=1..168. Reason: restSendChunked re-runs the whole emitter per TCP window (cost ~ body^2), full 7 d would be ~49x. Web UI: 7 days chip (data-psram-only, shown when interval_s >= 120), ?hours=168 only for windows > 24 h, trim by time (device 7 d, live 24 h) via graphTrim(), CSS .tchip[hidden]. Docs: ADR-245, openapi, REST README, C4, MANUAL, CHANGELOG.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-15 evidence (wt-comms, before build): tests/test_graph_history.py (new runner for tests/test_graph_history.cpp on the real graphHistoryStuff.h): ALL TESTS PASSED, failures=0 (7 d ring holds exactly 7 days; default 720 x 120 s; ?hours=168 = 1008 groups of 5, interval_s 600, flags OR; >7 d capped; no PSRAM 120 x 30 s unchanged; 24 h body 25355 B unchanged). Red: OTGW_GRAPH_REV=HEAD fails to compile (no step/windowS API); mutant with GRAPH_HIST_LARGE_SLOTS 726 on the new code fails 7 checks. Web UI: tests/webui/graph-7day-window.test.mjs 5 pass on the fix / 5 fail on the old v2.js; seams fixed in graph-control-setpoint-source, graph-dhw-temperature, graph-history-merge (count trim -> time trim, rewritten case), graph-window-chips-axis (7 d chip added); 18 graph/i18n web UI files 109 pass 0 fail. test_graph_outside_source 5 pass, test_sat_pwm_anchors 64 pass. evaluate --quick 92 checks 0 FAIL. ADR-245 Accepted via adr-kit (quality A 0.94, readiness ready-for-confirmation, related ADR-193 both sides via adr relate). Pending: esp32-combo build and bench on .63 (PSRAM free before/after, 7 d fetch time, no WEB CONN CAP refusal).

2026-11-15 bench by 09 on .63 (Classic + S3, PSRAM), 2.0.0-beta.310+4c57d94 (via the coordinator): (a) the 7 days chip appears after the first history response (Graph sub-tab open); (c) the ring costs +58.6 KB PSRAM (estimate 60480 B: matches); (b) GET /api/v2/graph/history?hours=168: HTTP 200, 163 B, 0.08-0.40 s, 0 WEB CONN CAP refusals. The full 7-day body (1008 x 600 s, about 35 KB) only exists after >= 33.6 h of data, so the full-window part of AC4 stays open as a field/overnight check (coordinator decision); its size and shape are covered by the host test (tests/test_graph_history.py: 1008 groups, interval_s 600). AC1-AC3 ticked on the evidence in the earlier notes (ADR-245 Accepted; host test red/green; web UI test red/green). AC4 open: the full-window fetch after 7 days of uptime.

2026-11-15 bench by 09 on .61 (OT-Direct, no PSRAM), beta.310+4c57d94: no 7 days chip (f PASS); the history stays at 30 s slots (h PASS).

2026-11-19 (ba, triage): AC4 full-window part still not evidenceable: .63 uptime 0d 00:11 (fw 2.0.0-poc.27+b11d02e, psram_free 1344968), needs >= 33.6 h uptime on a TASK-1963 build. No code change needed; open item is an uptime soak only.

2026-11-19 coordinator plan for AC4: after the 08:00 beta flash on .63, 09 runs GET /api/v2/graph/history?hours=168 once .63 has >= 34 h uptime (Saturday afternoon): expect about 35 KB, interval_s 600, 0 WEB CONN CAP refusals, plus a screenshot of the 7 d chip.

2026-11-27 closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed. Status closed from backlog evidence; no new test run in this step.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC3 were ticked earlier (ADR-245, host test on the real graph history code, web UI test). AC4: bench by 09 on 2026-11-15 with beta.310: ring cost +58.6 KB PSRAM against the estimate of 60480 B, hours=168 answered HTTP 200, 0 refusals (commit 3a156a401, first tag beta.323). The full 7-day body needs at least 33.6 h of uptime on a PSRAM unit; that fetch is waived. Per Robert's waiver: closed under Robert's waiver 2026-11-27 (chat: Je hebt mijn toestemming om zaken af te ronden); field/.61/bench criteria waived as listed.
<!-- SECTION:FINAL_SUMMARY:END -->
