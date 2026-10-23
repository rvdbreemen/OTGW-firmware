---
id: TASK-1328
title: >-
  Investigate the 732-byte heap watermark on a tester's OT-Direct unit
  (alpha.474)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-23T19:01:00Z'
updated_date: '2026-10-23T21:31:00Z'
labels:
  - heap
  - diagnostics
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 451000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D's capture 2026-10-23 21:30 (#alpha-testing msg 1563262919527895071, transcript-20261023-213057-2.0.0-alpha.4749ec1b87-...-OTGW-otgw-1020BA16C6BC.txt): telnet banner 'Heap : free 65996 frag 52% minFree 732 maxBlk 31732', uptime 12 min, 'low/warn/crit 1/0/0', 'maxLoopGap 1967 ms'. minFree is the native ESP32 allocator watermark: at some point in the first 12 minutes free heap dropped to 732 bytes, close to an allocation failure. reboot_log shows software resets 20:17/20:18 and a power-on 20:32. Find what allocates that much at boot or under load on this setup (BLE? MQTT discovery? web UI page load with graph history?) and whether the earlier resets were OOM-related. Ask for the stats topics (otgw-firmware/stats/heap) over a longer run if needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The cause of the low watermark is named with evidence (capture, telnet heap trace or bench reproduction), or recorded as not reproducible with what was tried
- [x] #2 If a firmware fix follows: watermark before/after on the same scenario
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-23 23:55 cause named with a bench reproduction. Capture analysis (Sergeant D, alpha.474): the capture script, on every telnet connect, finds all debug toggles on and sends 'q' (read settings) + 'D' (dump); during that dump free heap fell 73996 -> 50276 in 2 s, then 'MQTT TCP disconnected', telnet silent ~70 s, connection reset 20:32:15, reboot_log 'Power on' 20:32:35 (asked him whether he power-cycled). Earlier resets 20:17:29/20:18:41 (and 10-21 07:49:11/56) are 'Software reset (3)' with no crashlog: consistent with planned restarts after an update (safeboot/fs upload), not an OOM panic; not proven per entry. Bench OTGW32 (alpha.477, MQTT to temp broker, sat_boiler_emulator OT traffic), fresh reboot per run, 120 s: no telnet -> min_free 24696; telnet fast reader -> 4800; slow reader (2 KB/s) -> 3612. All toggles + q + D: trace heap 71408 -> 32260 within 0.5 s after ~10 KB output (maxBlk 31732 -> 20468), recovers to ~79 KB; the D dump alone (no readSettings) also dips to 47232; native watermark 776 bytes (his 732). So the low watermark is caused by bursts of telnet debug output, not by readSettings, BLE or discovery. Mechanism: AsyncSimpleTelnet write() -> _flushTx() does add(COPY)+send() per write, one small TCP segment per debug fragment. Which layer holds the bytes (lwIP pbufs / WiFi dynamic TX buffers / AsyncTCP events) is not split yet. Fix needs a vendored-library change or a debug-writer change -> TASK-1333 (maintainer decision). AC2: no firmware fix in this task, see TASK-1333 (before value 776 B recorded there). Bench restored: legacyport25238enabled false, temp broker stopped.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The 732-byte heap watermark on Sergeant D's alpha.474 unit is caused by bursts of telnet debug output: the capture script switches every debug toggle on and dumps the settings, and each debug fragment goes out as its own small TCP segment. Reproduced on the bench: minFree 24.7 KB without telnet, 3.6-4.8 KB with a debug session, 776 bytes with all toggles plus the q/D dump (his 732). The earlier software resets match planned update restarts, not an OOM crash. The fix (coalescing telnet sends, or one write per debug line) needs a maintainer choice and is filed as TASK-1333, together with the open question whether his unit wedged before the power-on.
<!-- SECTION:FINAL_SUMMARY:END -->
