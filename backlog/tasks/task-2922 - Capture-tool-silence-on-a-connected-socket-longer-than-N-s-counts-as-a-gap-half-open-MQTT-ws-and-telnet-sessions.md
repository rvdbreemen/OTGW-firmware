---
id: TASK-2922
title: >-
  Capture tool: silence on a connected socket longer than N s counts as a gap
  (half-open MQTT, /ws and telnet sessions)
status: To Do
assignee: []
created_date: '2026-11-19T16:26:00Z'
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
- [ ] #1 A connected stream (MQTT, /ws, telnet) that delivers nothing for longer than a per-stream limit is written to gaps.log as a gap with start/end, and the tool reconnects that stream
- [ ] #2 Host test with a fake server that accepts and then goes silent: the gap appears in gaps.log and the stream reconnects; red on today's tool
- [ ] #3 The limit is above the normal quiet time of each stream (telnet and MQTT can be quiet with SAT off), stated in the code with the reason
<!-- AC:END -->
