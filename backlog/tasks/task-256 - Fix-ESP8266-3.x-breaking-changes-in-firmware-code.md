---
id: TASK-256
title: Fix ESP8266 3.x breaking changes in firmware code
status: In Progress
assignee:
  - '@RvdB'
created_date: '2026-04-12 11:59'
updated_date: '2026-04-12 15:31'
labels:
  - esp8266
  - bugfix
  - build
dependencies: []
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Breaking changes in the firmware code that need fixing before or after the ESP8266 3.1.2 core upgrade. Based on analysis of the official 3.0.0-3.1.2 release notes and the OTGW firmware codebase. NOTE: user_interface.h DHCP functions (wifi_station_dhcpc_start/stop) are still present in 3.x SDK — that was a false alarm. The real issues are listed below.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Remove #include user_interface.h from networkStuff.h and replace wifi_station_dhcpc_start/stop() calls in networkStuff.ino:219 with equivalent WiFi-stack API that exists in ESP8266 3.x
- [x] #2 Remove axTLS namespace typedef from OTGW-ModUpdateServer.h:99-100 (axTLS is completely gone in 3.x, only BearSSL remains; the BearSSL equivalent is already present in the file)
- [x] #3 Replace ESP.getResetInfoPtr() usage in OTGW-firmware.ino:72 with the String-based ESP.getResetReason() API (rst_info pointer API removed in 3.x)
- [x] #4 Verify rtcUserMemoryRead/Write() in OTGW-firmware.ino:59,63 still compiles with 3.1.2 SDK; if removed, replace portal-reset flag with LittleFS-based flag
- [x] #5 Verify FSInfo struct and LittleFS.info() API still compile unchanged in 3.x (restAPI.ino, FSexplorer.ino, networkStuff.ino)
- [ ] #6 Fix axTLS namespace in OTGW-ModUpdateServer.h:99-100: remove the axTLS::ESP8266HTTPUpdateServerSecure typedef (axTLS fully removed in 3.0.0; BearSSL variant is already present in the same file)
- [ ] #7 Fix time_t format strings: scan all .ino/.h files for '%lu' or '%ld' used with time_t values — time_t is now 64-bit in 3.x (newlib 4.0), correct specifier is '%lld' or cast to (long long)
- [ ] #8 Replace ICACHE_RAM_ATTR with IRAM_ATTR in all .ino/.h files (deprecated in 3.x, emits warnings; grep for ICACHE_RAM_ATTR)
- [ ] #9 Verify WiFi-at-boot behaviour: in 3.x WiFi is OFF at boot by default. Check networkStuff.ino startup sequence — if WiFi.status() or localIP() is read before explicit WiFi.begin()/mode(), add enableWiFiAtBootTime() guard or reorder calls
- [ ] #10 Verify OOM safety: new that fails now calls abort() instead of returning nullptr. Scan for pattern 'new Foo; if (!ptr)' — those null checks are dead code in 3.x and may hide allocation failures
- [ ] #11 GCC 10.2 clean compile: after upgrading, review all new warnings (signed/unsigned mismatches, narrowing conversions, unused vars) — with --warnings default some may be errors
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis complete 2026-04-12 based on official 3.0.0-3.1.2 release notes and codebase scan.

CONFIRMED BREAKS:
- axTLS namespace: removed in 3.0.0 (OTGW-ModUpdateServer.h:99-100)
- time_t 64-bit: %lu/%ld format strings for time_t are wrong (GCC will warn or silently produce wrong output)
- ICACHE_RAM_ATTR: deprecated -> IRAM_ATTR

FALSE ALARMS (NOT breaking):
- user_interface.h / wifi_station_dhcpc_start/stop: still declared in SDK
- strlcpy_P, strcmp_P etc: all available in 3.x newlib
- FSInfo: no API change
- ESP.getResetInfoPtr(): needs compile test to confirm

NEEDS COMPILE TEST:
- rtcUserMemoryRead/Write in OTGW-firmware.ino:59,63
- WiFi-at-boot (subtle runtime issue, not compile error)
- OOM new nullptr checks (dead code, not compile error)
- GCC 10.2 stricter type checking (may surface existing warnings as errors)

- strlcpy_P helper added to OTGW-firmware.h (lines ~16-27) with #ifndef guard for forward compatibility
- OTGW-Core.ino:149 reverted to use strlcpy_P (clean call site)
- collectHeaders() API updated: FSexplorer.ino:219 uses new variadic String API
- jsonStuff.ino: 7x %d -> %u for (uint32_t)epoch casts
- axTLS namespace removed from OTGW-ModUpdateServer.h
<!-- SECTION:NOTES:END -->
