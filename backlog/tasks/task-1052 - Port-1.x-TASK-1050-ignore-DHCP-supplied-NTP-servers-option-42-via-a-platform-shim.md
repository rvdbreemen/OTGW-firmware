---
id: TASK-1052
title: >-
  Port 1.x TASK-1050: ignore DHCP-supplied NTP servers (option 42) via a
  platform shim
status: Done
assignee:
  - '@claude'
created_date: '2026-07-31 19:51'
updated_date: '2026-07-31 20:33'
labels: []
dependencies: []
ordinal: 247000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port of otgw-1.x.x commit 6eceed8f7 (v1.7.2). VERIFIED APPLICABLE ON ESP32-S3: D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/sdkconfig line 2819 has CONFIG_LWIP_DHCP_GET_NTP_SRV=y, so every DHCP renewal pushes the router NTP server (option 42) into the SNTP module, same leak class as the 1.x lwIP2 path. Field signature on 1.x was an uptime-locked heap onset at T1 lease renewal followed by reboot. The firmware runs its own NTP via startNTP(), so DHCP-supplied servers are unwanted regardless. Per the CLAUDE.md ESP platform abstraction rule this must NOT be a raw esp_sntp call in the .ino: add a platformIgnoreDhcpNtp() shim to platform_esp32.h (esp_sntp_servermode_dhcp(0)) and platform_esp8266.h (sntp_servermode_dhcp(0)) and call it unguarded from setup() before WiFi auto-connect can complete DHCP.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 platformIgnoreDhcpNtp() shim exists in BOTH platform_esp32.h and platform_esp8266.h
- [x] #2 setup() calls platformIgnoreDhcpNtp() before any WiFi/DHCP path, with a comment citing the sdkconfig evidence
- [x] #3 no raw esp_sntp/sntp symbol appears outside the platform headers
- [x] #4 build.bat green for esp32 target
- [x] #5 python evaluate.py --quick shows no new abstraction-boundary violations
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 amended in flight: platform_esp8266.h does NOT exist on this branch. dev is ESP32-S3-only (ESP8266 dropped 2026-06-20), and src/libraries/Platform/src/ contains only boards.h, platform.h and platform_esp32.h. The shim was added to platform_esp32.h alone; there is no second platform header to add it to. The AC was written from the CLAUDE.md abstraction rule, which describes a two-platform layout that no longer applies here.

platformIgnoreDhcpNtp() wraps esp_sntp_servermode_dhcp(0), with #include <esp_sntp.h> added alongside the other esp_* includes. Called as the FIRST statement of setup(), ahead of the runtime hardware detect and anything that can bring up WiFi, so no DHCP handshake can complete before it runs. No raw sntp/esp_sntp symbol appears outside the platform header (verified by grep). evaluate.py 94 checks / 0 failures, no new abstraction-boundary violations; build.bat --target esp32 SUCCESS for firmware and filesystem.

Evidence for applicability, since my first read of this was wrong: D:/DevData/platformio/packages/framework-arduinoespressif32-libs/esp32s3/sdkconfig:2819 has CONFIG_LWIP_DHCP_GET_NTP_SRV=y. I had initially assumed ESP-IDF defaulted this off and that the port was unnecessary.
<!-- SECTION:NOTES:END -->
