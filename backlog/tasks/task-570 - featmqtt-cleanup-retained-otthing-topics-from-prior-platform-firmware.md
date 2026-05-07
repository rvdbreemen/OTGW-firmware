---
id: TASK-570
title: 'docs(mqtt): note retained otthing/* topics for OTTHING-port testers'
status: To Do
assignee: []
created_date: '2026-05-07 19:36'
updated_date: '2026-05-07 19:37'
labels:
  - mqtt
  - migration
  - 2.0.0
dependencies: []
priority: medium
ordinal: 33000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Scope shrunk per maintainer (2026-05-07): do NOT add firmware migration code. Just add a tester-facing note in docs/guides/ explaining that retained otthing/<chipid>/* topics observed on the broker after flashing OTGW32 firmware are zombies from the prior OTTHING-platform firmware. Provide the mosquitto / HA cleanup recipe so testers can clear them once and never see them again. Current OTGW32 firmware does NOT publish under otthing/ (verified: only sTopTopic='OTGW' default in MQTTstuff.h:51).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On MQTT connect, firmware subscribes to otthing/+/+ for a bounded cleanup window (recommend 5-10s, mirroring mqttV2Migrat behaviour)
- [ ] #2 Each received retained payload under otthing/* is cleared via empty-retained publish to the same topic
- [ ] #3 After the cleanup window, firmware unsubscribes from otthing/* and never re-subscribes (cleanup is one-shot, not recurring)
- [ ] #4 If no retained otthing/* messages exist on the broker, firmware behaviour is unchanged (no errors, no extra log noise beyond a single 'starting otthing migration' line)
- [ ] #5 ADR drafted (or existing migration ADR amended) documenting the cleanup mechanism, the deprecation timeline, and when to remove the migration block (suggest 2 minor releases out)
- [ ] #6 ./build.sh --firmware exits 0 for both ESP8266 and ESP32 targets
- [ ] #7 python3 evaluate.py --quick — no new failures
- [ ] #8 Field validation: tester confirms otthing/<chipid>/* topics are gone from the broker tree after one MQTT-reconnect cycle
<!-- AC:END -->
