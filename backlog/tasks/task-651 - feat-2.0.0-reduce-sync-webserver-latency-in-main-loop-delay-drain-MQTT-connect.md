---
id: TASK-651
title: >-
  feat-2.0.0: reduce sync webserver latency in main loop
  (delay/drain/MQTT-connect)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-21 20:15'
updated_date: '2026-05-21 21:15'
labels:
  - performance
  - mqtt
  - otgw-2.0.0
dependencies: []
priority: high
ordinal: 49000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Three minimum-risk fixes to make httpServer.handleClient() reach the network stack more often on the 2.0.0 branch (which uses the synchronous ESP8266WebServer/WebServer, not async). Diagnosis in /root/.claude/plans/groovy-spinning-cray.md from prior session: HTTP latency floor = longest contiguous work between two handleClient() calls. Today that is dominated by (a) an unconditional 1ms delay every iteration, (b) an unbounded PIC serial drain that fans out per-line MQTT publishes, and (c) a 15-second blocking MQTT (re)connect that freezes the entire firmware when the broker is unreachable. This task ships the three low-risk fixes (audit items #1, #3, #4) together as a beta. Each fix lands as its own commit with its own _VERSION_PRERELEASE bump so Discord field testers can A/B them.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fix #1: remove unconditional delay(1) at end of doBackgroundTasks() in src/OTGW-firmware/OTGW-firmware.ino (~line 575); replace with yield() so loop frequency is not artificially capped at ~1kHz
- [x] #2 Fix #3: cap PIC serial drain to a small per-iteration line budget in handlePICSerial() (src/OTGW-firmware/OTGW-Core.ino ~line 4440) and in handleOTDirectBridgeStream() (src/OTGW-firmware/OTDirect.ino ~line 636), so a burst of OT frames no longer starves HTTP for 50-200ms
- [x] #3 Fix #4: shorten MQTTclient.setSocketTimeout from 15s and add non-blocking reconnect backoff in src/OTGW-firmware/MQTTstuff.ino MQTT state machine (~lines 1000-1110), so an unreachable broker no longer freezes the whole firmware for 15s per attempt
- [x] #4 Each of the three fixes is its own commit with its own _VERSION_PRERELEASE bump via bin/bump-prerelease.sh (testers can identify which beta carried which fix)
- [x] #5 python build.py --firmware returns exit 0 after each commit
- [x] #6 python evaluate.py --quick shows no new failures after each commit
- [x] #7 Draft PR opened against feature-dev-2.0.0-otgw32-esp32-sat-support with summary linking the three commits
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read each target file/region to confirm exact line numbers and surrounding context on 2.0.0 tip\n2. Fix #1: change delay(1) -> yield() at OTGW-firmware.ino end of doBackgroundTasks(); bump prerelease; build; commit\n3. Fix #3: introduce a small per-iteration line cap in handlePICSerial() and handleOTDirectBridgeStream(); bump prerelease; build; commit\n4. Fix #4: lower MQTTclient.setSocketTimeout, add exponential backoff between connect attempts; bump prerelease; build; commit\n5. python build.py --firmware and python evaluate.py --quick after each commit\n6. Push branch claude/mainloop-latency-2.0.0; open draft PR against feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation completed in three commits on branch claude/mainloop-latency-2.0.0:
- 527d25ec perf(loop): replace delay(1) at end of doBackgroundTasks() with yield() [alpha.49]
- dc0e1004 perf(loop): cap per-call line dispatch in PIC serial and 25238 bridge drains [alpha.50]
- 6ccc974a perf(mqtt): cut socket timeout 15s->5s and add growing retry backoff [alpha.51]

Local verification (build sandbox has no ESP32 toolchain due to SSL policy, so esp32 build deferred to CI):
- python build.py --firmware --target esp8266: exit 0 after each commit
- python evaluate.py --quick: 0 failures (Health Score 98.5%, 1 pre-existing warning) after each commit

Draft PR #618 opened against feature-dev-2.0.0-otgw32-esp32-sat-support. CI: evaluate.py already green; pio esp8266 + pio esp32 + claude-review still running at the time of close.

PR #618 merged into feature-dev-2.0.0-otgw32-esp32-sat-support (2026-05-21, merge commit 3f154d1d). Merge graph absorbed PR #619 (TASK-651 dev-line port) so Fix #1 reached the 2.0.0 line via the official squash from that PR; Fixes #3, #4, and the ESP32-flash slim landed via this PR's merge commit c5fdd638. Final ESP32 flash margin before merge: 281 bytes (up from 41 pre-rebase reconciliation).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Three commits land low-risk, high-impact fixes targeting the worst contiguous-work gaps between successive httpServer.handleClient() calls on the 2.0.0 sync-webserver branch.

Changes (one commit + one prerelease bump per fix so field testers can A/B them):
1. alpha.49 — replace unconditional delay(1) at the end of doBackgroundTasks() with yield(). Removes a 1ms-per-iteration floor that capped loop frequency at ~1kHz.
2. alpha.50 — cap PIC serial RX, legacy 25238 net->ser bridge, and OT-direct 25238 bridge to kMaxLinesPerDrain=4 dispatched lines per call. A burst of OT frames no longer freezes the loop for 50-200ms while processOT() fans out ADR-104 publishes.
3. alpha.51 — MQTTclient.setSocketTimeout 15s -> 5s + growing (3s/6s/9s/12s) backoff between failed connect attempts via CHANGE_INTERVAL_SEC. When the broker is unreachable, per-attempt freeze drops from 15s to 5s and the loop returns to HTTP service ~3x more often.

Out of scope (deliberately deferred): audit items #2 (interleave handleClient between heavy steps; needs static-buffer ownership audit first), #5 (per-iteration MQTT publish budget for OT-frame fanout), #6 (telnet byte-cap + discovery drip polish). These ship as a follow-up PR.

Verification: ESP8266 build clean after each commit; python evaluate.py --quick reports 0 failures (98.5% health, 1 pre-existing warning). ESP32 build deferred to CI (sandbox network policy blocks the toolchain download). Field validation will follow on Discord #beta-testing.

Draft PR: #618.
<!-- SECTION:FINAL_SUMMARY:END -->
