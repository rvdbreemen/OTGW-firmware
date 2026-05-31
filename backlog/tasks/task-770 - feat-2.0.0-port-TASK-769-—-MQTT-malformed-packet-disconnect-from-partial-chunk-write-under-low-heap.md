---
id: TASK-770
title: >-
  feat-2.0.0: port TASK-769 — MQTT malformed-packet disconnect from partial
  chunk write under low heap
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 06:38'
updated_date: '2026-05-31 07:06'
labels:
  - bug
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of dev TASK-769. Same streaming MQTT publish path (beginPublish->writeMqttChunk->endPublish) exists on the 2.0.0/ESP32 line. On a short-write the caller still calls endPublish() on a truncated payload, desynchronising the MQTT TCP stream -> broker "malformed packet" -> disconnect -> reconnect "session taken over".

ESP32 note: far more RAM than ESP8266, so the low-heap trigger is much less likely to fire in practice. The code-level defect (no clean abort on short-write) is still present and should be fixed for correctness and parity. Verify chunk helper + caller line numbers on this branch (they differ from dev) before editing.

Reported origin (dev): Discord #english-support, GeorgeZ83 (geo83_44083), ESP8266 fw 1.6.0. Milestone: 2.0.0.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 writeMqttChunk/writeMqttProgmemChunk short-write no longer leaves a partial MQTT packet on the wire: clean TCP drop (MQTTclient.stop) instead of endPublish() on a truncated payload
- [ ] #2 Bounded retry-with-yield on MQTTclient.write() short-writes so a started publish completes when sndbuf drains
- [ ] #3 Heap-guard threshold review aligned with dev decision; relax only after desync fix, record ESP32-specific values
- [ ] #4 ESP32 build (OTGW32 target) passes exit 0
- [ ] #5 Evaluator green / no new failures
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Locate the chunk helpers + callers on this branch (line numbers differ from dev) — beginPublish/writeMqttChunk/writeMqttProgmemChunk/endPublish in MQTTstuff.ino.
2. Same desync fix as dev TASK-769: bounded retry-with-yield() in chunk helpers; on genuine failure drop TCP via MQTTclient.stop() instead of endPublish() on truncated payload.
3. Build for OTGW32/ESP32 target per this worktree CLAUDE.md; evaluator green.
4. Commit MQTTstuff.ino only. Do NOT push — needs explicit confirmation (2.0.0 branch + field validation).
5. Guard-relax deferred to follow-up.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Verified 2026-05-31 (final). Fix committed as 8360e35 on feature-dev-2.0.0-otgw32-esp32-sat-support; NOT pushed (origin still at 8472189). Task left at In Progress per scope (AC#3 heap-guard review deferred; field validation pending).

CORRECTION HONORED: original spec said MQTTclient.stop() which does NOT compile (PubSubClient has no stop(); first build FAILED with "class PubSubClient has no member named stop" at 3 sites). All sites use MQTTclient.disconnect() (codebase idiom, MQTTstuff.ino:571/1039).

Committed MQTTstuff.ino marker counts (git show HEAD): MQTTclient.stop()=0, MQTTclient.disconnect()=6 (2 pre-existing + 4 fixed failure branches at lines 1291/1339/1647/1724), retry while-loops=2 (writeMqttChunk line ~290, writeMqttProgmemChunk line ~310, both bounded by MQTT_WRITE_MAX_RETRIES=10 at line 51 with feedWatchDog()+yield() between attempts), endPublish in writeMqtt* failure branches=0, success-path endPublish=6 (untouched).

Build receipt (python build.py --firmware): EXIT 0 -- esp8266 SUCCESS (RAM 88.5%, Flash 83.6%), esp32 SUCCESS (RAM 34.0%, Flash 95.2%), "Build completed successfully!". 0 error, 0 undefined reference, 0 FAILED, 0 no-member-stop.
Evaluator (python evaluate.py --quick): EXIT 0, 0 Failed, health 98.6%, 0 MQTTstuff findings (1 pre-existing WARN unrelated).

Files in commit (3): MQTTstuff.ino, version.h, data/version.hash. Prerelease alpha.111.
<!-- SECTION:NOTES:END -->
