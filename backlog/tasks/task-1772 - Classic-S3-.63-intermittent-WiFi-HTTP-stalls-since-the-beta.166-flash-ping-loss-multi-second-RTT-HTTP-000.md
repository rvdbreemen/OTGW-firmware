---
id: TASK-1772
title: >-
  Classic S3 (.63): intermittent WiFi/HTTP stalls since the beta.166 flash (ping
  loss, multi-second RTT, HTTP 000)
status: To Do
assignee: []
created_date: '2026-11-10T03:29:00Z'
updated_date: '2026-11-14T22:01:00Z'
labels:
  - bench
  - network
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 869000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Coordinator's overnight checks (2026-11-10): .63 (Classic + PIC, beta.166+c701f6b, flashed 23:14:33, bootcount 178, no reboot since) shows ping ~10% loss / max 3.5 s and intermittent HTTP 000 after 8-10 s; RSSI -61. Read-only re-measurement 04:2x by otgw-firmware-80: .63 ping 30x: 20% loss, RTT min 2 / avg 638 / max 2562 ms; GET /api/v2/device/info 10x (3 s apart): 10/10 200, max 4.8 s; uptime 05:14, heap 55 KB, max block 31,732 B. Same minute, .61 (OTGW32, beta.167): ping 0% loss, avg 92 / max 176 ms; device/info 9/10 200 (1 timeout), max 7.7 s. So .63 is clearly worse on ICMP, but .61 is not perfectly clean either. Not known: whether .63 behaved like this before beta.166 (no baseline in the session), whether it is radio/placement (RSSI -61 is moderate) or firmware (e.g. a busy loop/PIC serial load starving the WiFi task). Rule (Robert 2026-11-09): no analysis or fix against a description: capture first.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Capture: 30-60 min ping + device/info timing + telnet [HEAP]/loop-stall log on .63 and .61 side by side, plus the router's WiFi client stats for both MACs
- [ ] #2 Baseline: same measurement on .63 with the previous firmware (beta.165 or older) or a note why it cannot be done; verdict radio vs firmware with evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Correction 2026-11-10 (otgw-firmware-80): the .61 comparison above (1 timeout, max 7.7 s) is NOT usable: it ran around 04:25-04:26 and probably overlapped 4c's first TASK-1757 reboot of .61 at 04:26:44. Only the .63 numbers stand; .61 needs a clean re-measurement outside a reboot run.

2026-11-14 22:20-23:00 (09, AC1 partial, read-only, tools/stall1772.py): side by side 40 min, ping every 2 s + GET /api/v2/device/info every 10 s. .63 (beta.250+aed38bf, 1666 diag recording running): ping n=1200, loss 0, median 71 ms, p95 199, p99 332, max 907, 4 above 500 ms, 0 above 1 s; HTTP n=240, 0 errors, median 137 ms, p95 304, max 458; RSSI -70..-68. .61 (252 then 257; flash window 22:40:50-22:44:00 and a GW=L loopback 22:51:30-22:53:50 excluded): ping n=1033, loss 0, median 71, p95 124, p99 149, max 221, 0 above 500 ms; HTTP n=208, 0 errors, median 153 ms, p95 259, max 461; RSSI -62..-60. No stall, no HTTP 000, no ping loss on either unit in this window: the reported multi-second RTT / HTTP 000 did not reproduce tonight. .63 shows a heavier ping tail (p99 2.2x, 4 spikes 500-907 ms) at 8 dB lower RSSI and on a different AP (CAPsMAN cap) than .61 (router radio).

Gaps vs AC1 (not ticked): no telnet [HEAP]/loop-stall log on .63 (left out on purpose so the overnight 1666 recording is not disturbed); router snapshots via 'registration-table print detail' gave the association only, no signal/rate/ccq (needs 'print stats' next time). Next: repeat with telnet and router 'print stats' after the recording ends, and AC2 baseline needs a beta.165-era flash of .63. Evidence: 09 scratchpad r1772/ (ping-*.log, http-*.log, router.log, flash-window.txt).
<!-- SECTION:NOTES:END -->
