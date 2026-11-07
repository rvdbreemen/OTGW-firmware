---
id: TASK-1669
title: Free heap drifts ~500 B/h on the Classic+S3 during the beta.95 24 h soak
status: Done
assignee:
  - '@claude'
created_date: '2026-11-05T23:33:00Z'
updated_date: '2026-11-07T04:12:00Z'
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
- [x] #1 Full 24 h curve evaluated (linear vs levelling off) when the soak ends ~2026-11-06 15:00
- [x] #2 If linear: the growing allocation identified (heap tracing or per-consumer counters) and fixed, red/green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-06 01:42: soak sampler on .63 (beta.95, boot=148) was stopped by Claude Code's low-memory reaper after 10.5 h uptime; not restarted (needs Robert's ask). Data so far (scratchpad soak24-b95/samples.log): heap 54660 at 17:58 (up 3:01) -> 51116 at 01:28 (up 10:31) = -3544 B in 7.5 h, about 470 B/h, still linear; minfree 20760 -> 19736; maxblk 31732 flat; bootcount 148 unchanged; mqtt connected throughout. The 15:00 evaluation needs one manual device/info + crashlog + reboot_log read.

2026-11-06 fix (Robert chose: share the webhook task, 0 B extra RAM): SATweather.ino no longer creates/deletes a task per fetch; weatherRequestFetch() sets _wxRequested after webhookTaskAvailable(), the persistent webhook sender task waits at most 500 ms per queue receive and then calls weatherWorkerPoll() (webhook.ino); WebhookJob moved off the task stack to g_webhookJob (ADR-053) so the shared 8192 B stack keeps room for the fetch. Split test on .61 before the fix: 10 test webhooks (same HTTPClient chain, persistent task, no DNS) ~0 B (end 71164 -> 70984 -> 70984), so the per-fetch loss is the task create/delete (or DNS). Test tests/test_weather_worker_handoff.py: FIX 10/10 OK; OLD (HEAD SATweather.ino + webhook.ino) S3 FAIL (platformTaskCreatePinned found) and the compiled harness does not build. evaluate --quick 84 passed 0 failed.

2026-11-06 bench .61 beta.106+069ae27 (build OK, RAM 122916 B = +632 B .bss for g_webhookJob, which left the task stack; OTA exit 0, settings unchanged). Weather works on the new path: valid true, fetch_errors 0, forecast filled. Slope run with satweatherinterval=300 (12 fetches/h), REST only, 40 x 60 s samples (scratchpad b103/slope-fix.log): after boot settling the line is flat: from t=6 min slope -9 B/h (70968 -> 70708), from t=10 min -246 B/h (70692 -> 70708), noise band ~580 B; the old code at 12 fetches/h would lose ~1.4-1.6 KB/h (12 x 118-132 B). The same-condition run on the old code was killed by Claude Code's memory reaper after 4 min, so the old-code reference is the .63 soak (25/29 samples -132 B) and the .61 telnet probe (~107 B/fetch). Full suite: 159/166, the 5 failures also fail on the 704cc1e baseline (ota_flash, sat_corrections_index, capture_tool, boiler_unsupported_origin, local_frame_consumers) and 2 (static_integrity, refresh_storm) pass when run alone. Pushed; zip posted #alpha-testing 1568209227644928033. AC1 (24 h curve of the beta.95 soak) stays for the 15:00 read; then put beta.106 on .63 for an overnight curve.

2026-11-06 15:00 .63 (Classic + S3 Mini, beta.95+1fbd5cf): uptime 1(d)-00:03, bootcount 148 (unchanged since the soak start), mqttconnected true, crashlog available:false, reboot_log newest entries '2026-11-05 14:56:34 - reboot cause: Software reset (3)' and '2026-11-05 14:56:01 - ... [OTA] Rebooting...' (the beta.95 flash), nothing after: 24 h without a reboot. Heap: 54660 B at up 3:01 -> 44100 B at up 24:03 = -10560 B in 21.0 h (~503 B/h); minus the -676 B of the unauthorised research probe (TASK-1672) ~470 B/h; linear to the end, no levelling off; maxfreeblock 31732 flat, hd_min_free_heap 19736 unchanged, hd_min_max_block 17396. Matches ~4 x 118-132 B weather-fetch loss per hour (fixed in beta.106, AC2). Next: beta.106 on .63 for an overnight curve to confirm the slope goes to ~0 on the Classic.

2026-11-07 05:05 overnight curve on .63 (Classic+S3), beta.106+069ae27, read-only REST sampler every 15 min: 57 samples over 14.0 h, bootcount 150 throughout (no reboot), MQTT connected in every sample, heap 55308 -> 54040 B, least-squares slope -24 B/h (beta.95: about -470 B/h over 21 h). The 1 KB step is in the first 15 min after boot; flat afterwards. hd_min_free_heap 9160 B, min block 7668 B, unchanged all night.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause: a one-shot FreeRTOS task per Open-Meteo fetch lost ~132 B per fetch below our code. Fix (beta.106, c22f0b0fb): the weather fetch runs on the existing webhook task (weatherWorkerPoll every 500 ms), no task create/exit per fetch; tests/test_weather_worker_handoff.py S3 red on old, green on fix. AC1: 24 h soak of beta.95 measured ~470 B/h linear. AC2: .61 slope after the fix -9 to -246 B/h at 12 fetches/h, and the 14 h overnight curve on .63 beta.106: -24 B/h, no reboot, MQTT connected throughout (soak63-b106.log).
<!-- SECTION:FINAL_SUMMARY:END -->
