---
id: TASK-201
title: >-
  SAT: configTime() hostname reset workaround applies only to ESP8266 but has no
  platform guard
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:22'
updated_date: '2026-04-09 05:44'
labels:
  - audit-fix
dependencies: []
priority: low
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
networkStuff.ino startNTP() contains a workaround for configTime() resetting the WiFi station hostname on ESP8266 SDK (lines 313-330): it saves/restores the hostname and optionally restarts DHCP. On ESP32 configTime() does not reset the hostname, so the hostnameWasReset check is always false and platformRestartDHCP() is never called — safe but wasteful. More importantly, platformRestartDHCP() on ESP32 calls esp_netif_dhcpc_stop/start which is a heavier operation than the ESP8266 wifi_station_dhcpc_stop/start. The guard sDhcpHostnameFixed prevents re-entrant DHCP restart, so no stability risk exists, but the code is misleading on ESP32 and the comment implies an ESP8266-only SDK bug.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Wrap the hostname-reset DHCP workaround in #if defined(ESP8266) in networkStuff.ino:startNTP()
- [x] #2 Add a comment explaining why the workaround is not needed on ESP32
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Wrapped the configTime() hostname-reset workaround in networkStuff.ino:startNTP() with #if defined(ESP8266)/#endif guards. The pre/post-configTime hostname save/restore, hostnameWasReset detection, and platformRestartDHCP() call are now compiled out on ESP32, where configTime() does not touch the WiFi hostname. Added comment explaining the ESP8266-only SDK bug.
<!-- SECTION:FINAL_SUMMARY:END -->
