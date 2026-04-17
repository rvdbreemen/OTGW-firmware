---
id: TASK-283
title: 'Fix: v1.4.0-beta boot loop triggered by MQTT broker connection'
status: To Do
assignee: []
created_date: '2026-04-17 13:53'
labels:
  - bug
  - esp8266
  - mqtt
dependencies: []
references:
  - 'Discord #beta-testing, user mikdasa, 2026-04-17'
  - 'Discord #beta-testing, user crashevans, 2026-04-17 (confirmed)'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ESP8266 enters boot/crash loop when MQTT broker is reachable. Disconnecting broker stabilises the unit. doAutoConfigure throttle log shows 22 msgs dropped with heap=13168 bytes. Likely the new streaming autodiscovery overwhelms the ESP8266 when publishing many discovery entries in rapid succession. Confirmed by mikdasa and crashevans. Reverts to v1.3.10 fix the loop.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ESP8266 boots cleanly with MQTT broker connected
- [ ] #2 Autodiscovery completes without crash or throttle storm
- [ ] #3 Heap stays above safe threshold during discovery burst
<!-- AC:END -->
