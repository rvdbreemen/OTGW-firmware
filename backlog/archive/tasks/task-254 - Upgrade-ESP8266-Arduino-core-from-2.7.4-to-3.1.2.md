---
id: TASK-254
title: Upgrade ESP8266 Arduino core from 2.7.4 to 3.1.2
status: Done
assignee:
  - '@RvdB'
created_date: '2026-04-12 11:51'
updated_date: '2026-04-12 15:48'
labels:
  - build
  - esp8266
  - dependencies
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The firmware is pinned to ESP8266 Arduino core 2.7.4, which is several years old. Upgrading to 3.1.2 (current stable) unblocks ESPTelnet 2.x (uses WiFiServer::accept() added in 3.x), brings in WiFi stack improvements, and aligns with the current upstream.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 build.py updated to use ESP8266 3.1.2 core URL and version pin
- [x] #2 Firmware builds cleanly with ESP8266 3.1.2
- [x] #3 All breaking changes from 2.7.4 to 3.1.2 identified and resolved (WiFi API, user_interface.h, pgmspace, etc.)
- [ ] #4 feature/telnet-cli-welcome branch builds successfully after core upgrade is merged
- [x] #5 Fix user_interface.h: remove include and replace wifi_station_dhcpc_start/stop() with ESP8266WiFi stack equivalent (networkStuff.ino:219, networkStuff.h:38)
- [x] #6 Fix axTLS namespace: remove axTLS typedef in OTGW-ModUpdateServer.h:99-100 (axTLS is gone in 3.x, only BearSSL exists)
- [x] #7 Fix ESP.getResetInfoPtr(): replace rst_info* usage in OTGW-firmware.ino:72 with alternative (function removed in 3.x)
- [x] #8 Verify rtcUserMemoryRead/Write() still works in 3.x (OTGW-firmware.ino:59,63) or replace with file-based storage
- [x] #9 Verify FSInfo struct and LittleFS.info() still work in 3.x (FSexplorer.ino, restAPI.ino, networkStuff.ino)
- [x] #10 All pinned libraries verified compatible with ESP8266 3.1.2 (see TASK-255)
- [x] #11 Fix axTLS namespace in OTGW-ModUpdateServer.h:99-100 (TASK-256 AC1)
- [x] #12 Fix time_t format strings: %lu/%ld -> %lld for time_t values throughout codebase (time_t is now 64-bit in 3.x) (TASK-256 AC2)
- [x] #13 Replace ICACHE_RAM_ATTR with IRAM_ATTR (TASK-256 AC3)
- [x] #14 Verify WiFi-at-boot behaviour and OOM safety (TASK-256 AC4 and AC5)
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Fix CRITICAL breaking change: user_interface.h DHCP API (networkStuff.h:38, networkStuff.ino:219)
   - Remove #include "user_interface.h" from networkStuff.h
   - Replace wifi_station_dhcpc_start() with WiFi.begin() or equivalent 3.x API
   - Replace wifi_station_dhcpc_stop() with equivalent 3.x call

2. Fix CRITICAL breaking change: axTLS namespace removal (OTGW-ModUpdateServer.h:99-100)
   - Remove the axTLS::ESP8266HTTPUpdateServerSecure typedef
   - Keep only the BearSSL variant (already present)

3. Fix CRITICAL breaking change: ESP.getResetInfoPtr() removed (OTGW-firmware.ino:72)
   - Replace rst_info* pointer usage with ESP.getResetReason() String API
   - Or conditionally compile with #ifdef ESP8266_CORE_VERSION_NUM

4. Verify LIKELY BREAK: rtcUserMemoryRead/Write() (OTGW-firmware.ino:59,63)
   - Try to compile, check if these functions still exist in 3.1.2 SDK
   - If removed: replace with LittleFS-based portal reset flag

5. Verify LIKELY BREAK: FSInfo struct and LittleFS.info() API
   - Compile and check if FSInfo/LittleFS.info() signatures changed
   - Update if needed (restAPI.ino, FSexplorer.ino, networkStuff.ino)

6. Run first build attempt: python build.py --firmware
   - Document all remaining compiler errors
   - Fix iteratively

7. Full build + smoke test
   - python build.py
   - Flash to device, verify WiFi connect, MQTT, OT traffic, REST API
   - Verify telnet still works
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis complete 2026-04-12.

False alarms from initial codebase scan (NOT breaking in 3.x):
- user_interface.h + wifi_station_dhcpc_start/stop: STILL DECLARED in 3.x SDK
- strlcpy_P, strcmp_P, memcmp_P: all present in 3.x newlib
- FSInfo struct / LittleFS.info(): no breaking API change confirmed
- ESP.getResetInfoPtr(): not confirmed removed (needs compile test)
- rtcUserMemoryRead/Write: not confirmed removed

Actual breaking changes:
- axTLS namespace: CONFIRMED REMOVED in 3.0.0 (OTGW-ModUpdateServer.h:99)
- time_t now 64-bit: format strings %lu/%ld for time_t are wrong in 3.x
- ICACHE_RAM_ATTR: deprecated, use IRAM_ATTR
- WiFi OFF at boot: verify startup sequence
- GCC 4.8 -> 10.2: stricter type checking

Library situation: only WebSockets needs upgrading (2.3.6->2.7.2). See TASK-255.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
ESP8266 Arduino core upgraded from 2.7.4 to 3.1.2. All breaking changes resolved (see TASK-256). WebSockets updated to 2.7.2 (see TASK-255). Firmware builds cleanly on esp8266:esp8266@3.1.2 / GCC 10.2. AC4 (feature/telnet-cli-welcome builds after merge) remains open — that branch is still blocked on ESPTelnet 2.x which is unblocked by this upgrade.
<!-- SECTION:FINAL_SUMMARY:END -->
