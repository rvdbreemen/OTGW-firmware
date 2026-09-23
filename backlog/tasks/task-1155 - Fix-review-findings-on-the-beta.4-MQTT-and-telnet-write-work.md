---
id: TASK-1155
title: Fix review findings on the beta.4 MQTT and telnet write work
status: In Progress
assignee:
  - '@claude'
created_date: '2026-09-23 05:26'
updated_date: '2026-09-23 05:27'
labels:
  - bug
dependencies: []
priority: high
ordinal: 229000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Self-review of TASK-1148 and TASK-1154 after beta.4 shipped found five defects.

1. mqtt_desync_drops counts 2 of ~13 desync remedy sites. Uncounted: MQTTstuff.ino PROGMEM payload and sendMQTT() payload sites, and all nine discovery composers in mqtt_configuratie.cpp, which call client.disconnect() on a PubSubClient reference. The composers carry the largest payloads. mrfox7688 was asked to read this counter on GH #682.
2. The SimpleTelnet bounded retry (TASK-1148 part B) is inert by construction. SimpleTelnet sets the client write timeout to 1000 ms (SimpleTelnet_impl.tpp setTimeout(_keepAliveInterval)), and ClientContext::_write_from_source only returns a short count after that much time without progress, so the 2 ms budget is always exhausted before a second write. The earlier 133 B to 0 B evidence was n=1 against n=4 on a phenomenon known to be noisy.
3. writeMqttChunk and writeMqttProgmemChunk retry up to MQTT_WRITE_MAX_RETRIES = 10 with no wall-clock bound, and each write can block up to WiFiClient's 5000 ms no-progress timeout, so a stalled socket can hold the cooperative loop for about 50 s while the PIC serial line is not drained.
4. The re-entrancy rationale is false on 1.x: feedWatchDog() has its yield() commented out and doBackgroundTasks() is only called from loop(). The SimpleTelnet guard comment and CLAUDE.md both assert the non-existent path.
5. iMqttSndbufSkips and iMqttDesyncDrops are uint16_t, while the same struct's TASK-1039 note says counters read over time must not wrap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every desync remedy site increments mqtt_desync_drops, through one shared helper, verified by a grep that finds no bare desync disconnect left
- [ ] #2 The inert SimpleTelnet retry is removed rather than left in place, behaviour-neutral, with the honest byte count and the drop counter kept
- [ ] #3 writeMqttChunk and writeMqttProgmemChunk can no longer hold the loop for more than one socket timeout
- [ ] #4 The false re-entrancy claim is corrected in code comments and CLAUDE.md, with the evidence cited
- [ ] #5 Both counters are uint32_t
- [ ] #6 CHANGELOG and the TASK-1148 and TASK-1154 records no longer overclaim
- [ ] #7 python build.py --firmware exits 0, python evaluate.py --quick shows no new failures, and the host tests pass
<!-- AC:END -->
