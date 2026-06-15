---
id: TASK-875
title: >-
  fix(mqtt): onMqttMessage must gate on len==total, not just index!=0 (F4,
  ADR-131 item 8)
status: To Do
assignee: []
created_date: '2026-06-15 14:29'
labels: []
dependencies: []
ordinal: 91000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
MQTT review F4 (MEDIUM, latent). onMqttMessage (MQTTstuff.ino:1039) implements only 'if (index != 0) return;' but ADR-131 Decision item 8 mandates 'index==0 && len==total'. espMqttClient splits a PUBLISH payload on TCP read boundaries (Parser.cpp), so a payload can arrive as multiple onMessage calls even below EMC_RX_BUFFER_SIZE; the first chunk (index==0, len<total) passes the guard and is dispatched TRUNCATED, remaining chunks dropped. Latent today (all subscriptions tiny; verify-handler matches on topic name only), but a documented-contract violation and a truncation hazard for any future payload-bearing subscription. Fix: 'if (index != 0 || len != total) return;'.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 onMqttMessage gates on index==0 && len==total per ADR-131 item 8
- [ ] #2 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->
