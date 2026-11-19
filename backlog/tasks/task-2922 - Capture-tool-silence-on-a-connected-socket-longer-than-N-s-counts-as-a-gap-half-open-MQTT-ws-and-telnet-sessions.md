---
id: TASK-2922
title: >-
  Capture tool: record a capture-host sleep (wall clock jumps past the run
  clock) as a gap in gaps.log
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-19T16:26:00Z'
updated_date: '2026-11-19T17:25:00Z'
labels:
  - tools
  - capture
milestone: 2.1.0
dependencies: []
priority: low
ordinal: 1060000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in indigo_light's macOS upload 20261119-091712 (beta.382+224e30c), 2026-11-19. After a power cut at ~13:57 the tool's MQTT session (broker 192.168.1.21) and the /ws OT-frame session (gateway 192.168.20.191) stayed half-open: mqtt.log has 0 lines from 13:57:23 to 14:18:48 and otframes.log 0 frames from 13:57:24 to 14:18:48, while the gateway's telnet shows it publishing MQTT and handling OT frames at 14:07:40. Telnet reconnected at 14:06:55 and then had no line from 14:07:41 to 14:18:47 (668 s). At 14:18:47 all three sessions dropped in the same second (Mac side; the Mac<->broker session drops too, so not the gateway). gaps.log lists only 13:57:24-14:06:55 telnet 'read failed' and the 1 s gaps at 14:18:47: the 21 min MQTT/ws silence and the 11 min telnet silence are missing, so a reader takes a capture-side outage for complete data. The 60 s idle check did not fire on the half-open socket.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A watcher compares the wall clock with the run clock (time.monotonic) once a second; a jump of more than 30 s is written to gaps.log as one gap (stream capture-host, start, end, seconds, reason) and to the transcript
- [x] #2 Host test with injected clocks: a 668 s jump writes the gap, normal ticks and a small skew write nothing; red on today's tool (no watcher)
- [x] #3 Started in every run, joined at stop; documented in the gaps.log description
- [ ] #4 script.error.log is in the upload zip (test on the zip contents)
- [ ] #5 A connected stream (MQTT, /ws, telnet) that delivers nothing for longer than its own limit is written to gaps.log as a gap and reconnected; host test with a fake server that accepts and then goes silent, red on today's tool
- [ ] #6 The cause of the snapshot poller writing nothing from 13:53:36 to 14:19:01 is found or stated as not found, with indigo's script.error.log read
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 re-scoped (otgw-dev, before any code): the silence was not a half-open socket but most likely the capture Mac asleep. Evidence from the zip: SnapshotCapture waits 300 s of time.monotonic between rounds and writes an ERROR line when a request fails; snapshots.log has 13:48:36 and 13:53:36, then no line at all (no ERROR) until 14:19:01. The awake time 13:53:36-13:57:24 + 14:06:53-14:07:41 + 14:18:47-14:19:01 is about 4 min 50 s, one interval within the boundary uncertainty. On macOS time.monotonic does not advance during sleep (CPython uses mach_absolute_time there; not verified on a Mac in this session). So per-stream silence limits would not help (no code runs while asleep); the tool can only notice afterwards that the wall clock moved ahead of the run clock. Reconnecting after wake is left to the OS reset seen at 14:18:47 and the existing reconnect paths.

2026-11-19 CORRECTION (otgw-dev): the re-scope to 'capture Mac asleep' was WRONG. indigo_light refuted it in #beta-testing (1573025696236961873, relayed by 92; 92's public correction 1573025984549093507): the lid was never closed and the Mac was awake; his script.error.log logs 'No route to host' every 30-70 s from 09:32 to 13:56, and the power cut at ~13:57 also took down the wireless bridge the Mac reaches the network through; 14:07-14:18 and the drop at 14:18:47 are that bridge coming back. So it was a network outage, my first reading. The snapshot argument does not hold: the poller stopped writing (no ERROR line either) from 13:53:36 to 14:19:01 for a reason not yet found; per 92 the error lines in script.error.log also stop at 13:56, which points at the whole capture process going quiet, not one socket (open: macOS App Nap or a blocked HTTP_SLOT holder are guesses, not verified). SleepWatch (166eace70) stays: harmless, and a sleeping capture machine does happen elsewhere, but it did not apply here and its AC evidence only proves the sleep case. script.error.log is not in the upload zip (92 checked the unpacked folder); with it this would have been answered at once.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
scripts/otgw_capture.py gets SleepWatch (TASK-2922): once a second it compares the wall clock with time.monotonic; a jump above SLEEP_JUMP_SECONDS (30 s) is one gap of the pseudo stream capture-host in gaps.log (start = last wall time + the run time that passed, end = now, seconds, reason 'capture machine asleep or suspended') plus an index count and a transcript summary line; a wall clock step backwards is ignored. Started in every run next to the snapshot poller and joined at stop through the workers list. AC1/AC2: tests/test_capture_sleep_watch.py drives the real SleepWatch.check() with injected clocks: red on the old tool (AttributeError: no SleepWatch, 4 of 4), green 4/4 (a 668 s sleep = one gap 14:07:42-14:18:50 668 s, 120 normal ticks + a 12 s skew = nothing, a 1 h step back = nothing, the thread stops with the capture). AC3: thread start and join in main; scripts/README.md 'keep the PC awake' note names the capture-host gap. Regression: capture test files one by one: test_capture_py39_compat 3/3 (Python 3.9 of macOS), device_data 21, discovery 34, launcher 7, mqtt_own_topics 8, otframes 7, upload_size 3, flash_zip_capture_imports 3 (run_tests.py); test_capture_tool first run 4 failed / 57 passed (names not captured, under load), rerun 61 passed 2 skipped with no change in between, so counted as the known flaky class (TASK-2912), not verified further. evaluate.py --quick 88 passed 0 failed. Not verified: a real sleep on a Mac (the claim that time.monotonic stops during macOS sleep rests on the CPython implementation and on indigo's snapshot timing, not on a run here).
<!-- SECTION:FINAL_SUMMARY:END -->
