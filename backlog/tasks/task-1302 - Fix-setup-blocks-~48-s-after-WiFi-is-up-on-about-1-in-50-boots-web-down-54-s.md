---
id: TASK-1302
title: >-
  Fix: setup() blocks ~48 s after WiFi is up on about 1 in 50 boots (web down 54
  s)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-22T14:49:00Z'
updated_date: '2026-10-29T05:17:00Z'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 256000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen twice on the bench (.68) during the TASK-1262/TASK-1301 reset series, about 2 in 110 RTS resets: the web interface came back after 54 s although WiFi was up at 0.6-1.3 s (GotIP in the diag ring).
Diag ring 15:54:52 (fix build 87da863 + stage markers): ev35 after startWebserver() at 1354 ms, next marker ev38 after updateRebootLog() at 49894 ms. The markers for startWebSocket() and startMQTT() were accidentally inside a comment, so the ~48.5 s gap covers startWebSocket(), startMQTT() (its INIT state runs WiFi.hostByName("homeassistant.local") synchronously), initWatchDog() (I2C), ESP.getResetReason(), updateRebootCount() and updateRebootLog() (LittleFS writes). The first case (14:35:21, 52 s) came after 6 h of uptime; bench MQTT password missing (auth fails) in both.
A field gateway that hits this is unreachable for ~50 s after a restart, and the loop (OT handling) does not run meanwhile.
Diag patch with corrected markers (ev36, ev37, ev42-ev46 incl. around hostByName): scratchpad t1262/diag_plus_fix1301.patch (temporary, not for commit).
<!-- SECTION:DESCRIPTION:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Diag build = HEAD (1301 + 1290) + stage markers (diag patch applied 3-way, not committed), OTA to .68, MQTT auth still failing as in both slow boots.
2. RTS reset series until a slow boot is captured; the ring names the blocking stage.
3. Fix that stage, then 100+ resets old build vs fix on the same rig.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
- 2026-10-22 18:10-18:52: diag build 1.7.8-beta.6+c7b20e5 (scratchpad t1262/diag1302.ino.bin/.elf) on .68. 100 RTS resets (diag1302_a..e.out): all web up in 4-8 s, lease OTGW, 0 slow. Earlier rate 2/110, so 0/100 is not yet meaningful (P about 0.16 at that rate).
- Code read: startMQTT() only runs INIT (hostByName, core timeout 10 s), so it cannot explain 48 s alone; initWatchDog() is one 100 ms delay plus two I2C transfers; updateRebootLog() on reason 6 (RTS reset) calls initWatchDog() again, plus loopNTP() and LittleFS rewrite. FS 43% used.
- Running: up to 150 more resets, stop at first slow (diag1302_f.out).

- 2026-10-22 18:58-19:54: 150 more resets on the same diag build (diag1302_f.out): 0 slow. Total 250 resets on HEAD (1301 + 1290) + markers, web up in 4-9 s every time (4 s: 59, 5 s: 67, 6-9 s: 24 of the last 150). At the earlier rate of 2/110 that is about 1% likely, so something changed since the two slow boots.
- Both slow boots ran builds WITHOUT TASK-1290 (GARP). A/B started 20:00: same diag build with only the stationKeepAliveSetIntervalMs() call commented out (1.7.8-beta.6+12975b4, scratchpad t1262/diag1302_nogarp.*), up to 200 resets, stop at first slow (diag1302_nogarp.out). Not committed; the comment-out is a temporary bench change.

- 2026-10-22 20:17: no-GARP A/B stopped by Claude Code after 40 resets (system low on memory, commit charge 89%), not by the test. 40/40 normal (web 4-8 s), 0 slow. Bench .68 left on the no-GARP diag build 1.7.8-beta.6+12975b4. Resume only on maintainer request.

2026-10-28: uncommitted TEMP DIAG (stage markers + no-GARP A/B) removed from the 1.x worktree to merge origin; parked as %LOCALAPPDATA%\OTGW-capture\parked\task1302-temp-boot-diag-20261028.patch (git apply --check after the merge: see session). Bench .68 still runs the no-GARP diag build 1.7.8-beta.6+12975b4.

Post-merge check: code hunks still apply (git apply --check --exclude='*version.h' --exclude='*version.hash'); only the generated version files conflict, regenerate them instead.

2026-10-28: bench .68 will be reflashed with the beta.8 candidate (TASK-1434/1431 verification), replacing the no-GARP diag build.

2026-10-29: closed as 'cannot reproduce on current builds' (maintainer decision, relayed via the 2.0.0 session otgw-firmware-c5): 0/250 slow boots on HEAD with GARP plus stage markers, 0/40 with GARP commented out, against 2/110 earlier. ACs explicitly dropped. Diag patch stays parked at %LOCALAPPDATA%\OTGW-capture\parked\task1302-temp-boot-diag-20261028.patch. Bench .68 now runs released beta.8 (no diag build). Reopen with a new capture if a slow boot is seen again.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closed as cannot reproduce on current builds; no firmware change.

Two slow boots (setup() blocked ~48 s after WiFi up, web down 54 s) were seen in 110 resets on older builds. On builds since then: 0 slow in 250 RTS resets on HEAD with stage markers, and 0 slow in 40 resets with the TASK-1290 gratuitous ARP commented out. At the earlier rate 0/250 is about 1% likely, so something changed, but the blocking call was never captured.

Code read ruled out startMQTT() alone (one hostByName, 10 s timeout) and initWatchDog() (100 ms plus two I2C transfers). The stage-marker diag patch is parked under %LOCALAPPDATA%\OTGW-capture\parked\ for a future capture. ACs dropped by maintainer decision.
<!-- SECTION:FINAL_SUMMARY:END -->
