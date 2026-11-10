---
id: TASK-1757
title: >-
  Boot-time heap falls below CRITICAL (1.1-3.6 KB) when a browser loads the UI
  right after boot; mid-body resets on '/'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09T17:05:00Z'
updated_date: '2026-11-10T04:16:00Z'
labels:
  - heap
  - bug
  - webui
  - 2.0.0-stable-gate
dependencies: []
priority: high
ordinal: 859000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
2.0.0 stable gate. Found in TASK-1528 step b / TASK-1675 R1 (bench .61, beta.162+5786928, 2026-11-09, 10 reboots, run-20261109). Clean boots stay at 22-33 KB in the boot window. With the two-tab trace (2 tabs, '/' then '/favicon.ico', from the first HTTP answer after boot, 120 s) the windowed heap minimum falls to 1.1-3.6 KB, below the CRITICAL tier (3072 B), and 3 requests for '/' were reset mid-body at boot +8, +19 and +20 s, in the low-heap boots. 0 hangs. A user who opens the web UI right after an update or reboot hits this window. Robert, 2026-11-09: TASK-1675 stays open; this task carries the fix. Related TASK-1675 and TASK-1528.

Fix idea for the plan only (not decided): hold back UI serving (e.g. 503 + Retry-After, or a small static wait page) until '[HEAP] boot done'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce with ONE normal browser tab loading the UI right after boot first (not the two-tab trace): windowed heap minimum per boot phase and any reset, on the current dev build
- [ ] #2 Name which allocations in the boot window combine with UI serving to reach below CRITICAL, with evidence
- [ ] #3 Fix agreed with Robert in the plan; after the fix the one-tab and two-tab boot runs stay above CRITICAL with 0 mid-body resets, with old-vs-fix numbers
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
AC1 measurement (no code change), on .61 in a slot from the coordinator, on the dev build then on .61 (no flash if it already runs current dev; otherwise OTA first and restart counting after the flash). Runner: scratchpad t1757/boots1757.py, based on detectie's TASK-1528 step b runner (same /ReBoot loop, device/info every 5 s, telnet [HEAP] phase lines until '[HEAP] boot done'). New mode B: ONE real headless Chrome tab (fresh profile, --log-net-log) opens http://.61/ at the first HTTP answer after boot and stays 120 s, so the real v2 UI loads its files, polls REST and opens its WebSocket; failed requests come from the net-log (net_error at REQUEST_ALIVE end; self-test: ERR_CONNECTION_REFUSED -102 detected, 13 requests counted). After every boot the crashlog summary is saved (doubles as the TASK-1763 panic count). Sequence: 12 boots alternating C and B (C B C B ...), so per mode 6 boots. Before: check no other telnet logger holds port 23 (single slot) and stop only my own afterwards. Output per boot: windowed minimum per [HEAP] phase (setup-late, wait-mqtt, connect, drip), nativeMin, the boot +t of the minimum, failed browser requests with URL and error, crashlog line. Decision rule: if B stays above CRITICAL (3072 B) in all 6 boots, AC1 says the normal one-tab case is safe and the gate rests on the two-tab case; if B goes below, AC1 is reproduced with one tab. AC2 next: add allocation attribution only where the phase data points (e.g. which phase overlaps the browser's '/' 175 KB gzip send: AsyncTCP send buffers vs MQTT connect + discovery drip), measured, not assumed. AC3: fix options (e.g. 503+Retry-After or a tiny wait page until boot done, or deferring the HA discovery drip while a page load is in flight) go to Robert with old-vs-fix numbers from the same runner (one-tab B and two-tab T).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-09 ~23:00: the AC1 .61 slot tonight is cancelled (Robert: long soak of beta.166 on .61 and .63 until the ~05:00 public beta). The AC1 run (runner scratchpad t1757/boots1757.py, 12 boots C/B) moves to after that release, on the then current dev build.

AC1 result, 2026-11-10 04:28-05:15, .61 on 2.0.0-beta.167+e79e495 (OT-Direct bench unit, no PSRAM), runner scratchpad t1757/boots1757.py + summary1757.py, data run-20261110b. 12 reboots via /ReBoot, C (clean) and B (ONE real headless Chrome tab on http://.61/ from the first HTTP answer for 120 s: the real v2 UI loads, polls REST, opens /ws) alternating. All 12 boots clean: each up 15.3-16.0 s after /ReBoot, bootcount +1 each (203..214), '[HEAP] boot done reason=drip-done' in every boot (~3.5 min). Lowest windowed heap per boot (allWinMin; nativeMin equal except boot 9): C 21932 / 24156 / 18608 / 21380 / 19160 (native 18720, outside=1) / 10396; B 14648 / 10692 / 11800 / 13260 / 26108 / 9768. Lowest phase is 'connect' (WiFi/MQTT connect window) in 11 of 12 boots (drip in boot 9). Medians: C ~20.3 KB, B ~12.5 KB; minimum C 10.4 KB, B 9.8 KB. CRITICAL (3072 B) never reached; one normal tab does NOT reproduce the 1.1-3.6 KB of the two-tab trace in TASK-1528 step b. Browser (net-log): 96-100 requests per B boot; failures: ws://.61/ws net error -173 once in every B boot (not yet tied to the Chrome stop at +120 s; to check), and once /api/v2/otdirect/overrides -118 (connection timed out) in boot 10. No mid-body reset of '/' and no failed static file in any B boot. Crashlog: no new panic in 12 boots (TASK-1763 not reproduced in this run); the old 2026-11-09 12:02 entry rotated out of /reboot_log.txt after boot 6 (the coredump itself is archived, see TASK-1763). Side finding: device/info answers bootcount 0 for about the first second after boot (startWebserver() runs before updateRebootCount() in setup(), OTGW-firmware.ino:516 vs later), seen in boots 3, 4, 6. Next: AC2 (which allocations combine in the connect window, with the two-tab case as the reproducer, since one tab stays above CRITICAL) and a decision for Robert whether the one-tab result changes the 2.0.0 stable gate.
<!-- SECTION:NOTES:END -->
