---
id: TASK-268
title: 'Fix: ESP8266 v1.4.0-beta reboot loop after flash'
status: To Do
assignee: []
created_date: '2026-04-13 22:18'
updated_date: '2026-04-13 22:18'
labels:
  - bug
  - esp8266
  - critical
dependencies: []
references:
  - 'Discord #beta-testing'
  - user crashevans
  - '2026-04-13'
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by crashevans in Discord #beta-testing (2026-04-13). After flashing v1.4.0-beta (FS first then firmware) the ESP8266 enters a continuous reboot loop with an exception. MQTT still connects briefly between reboots, but the web UI is completely unresponsive. Reverted to v1.3.10 and device works normally again (WiFi 86%). Reporter also shared that pressing any number in telnet hangs the device. Screenshot of crash log shared as image attachment in Discord.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 ESP8266 boots cleanly after flashing v1.4.0-beta FS + firmware
- [ ] #2 Web UI accessible after flash
- [ ] #3 No reboot loop visible in telnet logs
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-13: crashevans reports device stuck in reboot loop with exception after v1.4.0-beta. MQTT visible between reboots. Telnet hangs when pressing keys. Shared screenshot but content not available in Discord read. Reverted to v1.3.10 and stable.
<!-- SECTION:NOTES:END -->
