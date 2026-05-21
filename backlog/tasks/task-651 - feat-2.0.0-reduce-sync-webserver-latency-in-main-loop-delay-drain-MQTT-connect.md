---
id: TASK-651
title: >-
  feat-2.0.0: reduce sync webserver latency in main loop
  (delay/drain/MQTT-connect)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-21 20:15'
updated_date: '2026-05-21 20:16'
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
- [ ] #1 Fix #1: remove unconditional delay(1) at end of doBackgroundTasks() in src/OTGW-firmware/OTGW-firmware.ino (~line 575); replace with yield() so loop frequency is not artificially capped at ~1kHz
- [ ] #2 Fix #3: cap PIC serial drain to a small per-iteration line budget in handlePICSerial() (src/OTGW-firmware/OTGW-Core.ino ~line 4440) and in handleOTDirectBridgeStream() (src/OTGW-firmware/OTDirect.ino ~line 636), so a burst of OT frames no longer starves HTTP for 50-200ms
- [ ] #3 Fix #4: shorten MQTTclient.setSocketTimeout from 15s and add non-blocking reconnect backoff in src/OTGW-firmware/MQTTstuff.ino MQTT state machine (~lines 1000-1110), so an unreachable broker no longer freezes the whole firmware for 15s per attempt
- [ ] #4 Each of the three fixes is its own commit with its own _VERSION_PRERELEASE bump via bin/bump-prerelease.sh (testers can identify which beta carried which fix)
- [ ] #5 python build.py --firmware returns exit 0 after each commit
- [ ] #6 python evaluate.py --quick shows no new failures after each commit
- [ ] #7 Draft PR opened against feature-dev-2.0.0-otgw32-esp32-sat-support with summary linking the three commits
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read each target file/region to confirm exact line numbers and surrounding context on 2.0.0 tip\n2. Fix #1: change delay(1) -> yield() at OTGW-firmware.ino end of doBackgroundTasks(); bump prerelease; build; commit\n3. Fix #3: introduce a small per-iteration line cap in handlePICSerial() and handleOTDirectBridgeStream(); bump prerelease; build; commit\n4. Fix #4: lower MQTTclient.setSocketTimeout, add exponential backoff between connect attempts; bump prerelease; build; commit\n5. python build.py --firmware and python evaluate.py --quick after each commit\n6. Push branch claude/mainloop-latency-2.0.0; open draft PR against feature-dev-2.0.0-otgw32-esp32-sat-support
<!-- SECTION:PLAN:END -->
