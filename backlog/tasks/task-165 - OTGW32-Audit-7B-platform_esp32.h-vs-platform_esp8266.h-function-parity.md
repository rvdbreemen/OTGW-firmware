---
id: TASK-165
title: 'OTGW32-Audit-7B: platform_esp32.h vs platform_esp8266.h function parity'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-08 22:22'
updated_date: '2026-04-08 22:31'
labels:
  - audit
  - otgw32
  - phase-7
milestone: m-1
dependencies: []
references:
  - src/OTGW-firmware/platform_esp32.h
  - src/OTGW-firmware/platform_esp8266.h
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Compare platform_esp32.h and platform_esp8266.h to verify that every platform function declared in one file has an equivalent in the other. Functions like platformRestartDHCP(), OT command routing, and hardware-specific initialization must have matching signatures. Any function present on one platform but missing on the other is a portability gap.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Every function in platform_esp32.h has a counterpart in platform_esp8266.h
- [x] #2 Every function in platform_esp8266.h has a counterpart in platform_esp32.h
- [x] #3 Function signatures match between platforms
- [x] #4 No shared code calls platform functions that only exist on one platform without a guard
- [x] #5 Any parity gap results in an audit-fix task
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
## Audit findings

Compared all inline functions in both platform headers.

**Functions in ESP32 but NOT in ESP8266:**
None found — all functions present in ESP32 are also in ESP8266.

**Functions in ESP8266 but NOT in ESP32:**
None found — all functions present in ESP8266 are also in ESP32.

**Full function inventory (both files match):**
- platformSetHostname(const char*)
- platformGetHostname()
- platformFSInfo(FSInfo&)
- platformCoreVersion()
- platformSdkVersion()
- platformCpuFreqMHz()
- platformGetMacAddress(uint8_t*)
- platformChipId()
- platformResetReason(char*, size_t)
- platformFreeHeap()
- platformMaxFreeBlock()
- platformFlashChipRealSize()
- platformFlashChipSize()
- platformFlashChipSpeed()
- platformFlashChipId()
- platformFlashChipMode()
- platformSketchSize()
- platformFreeSketchSpace()
- platformRestart()
- platformHardwareRandom()
- platformRtcRead(uint32_t, uint32_t*, size_t)
- platformRtcWrite(uint32_t, const uint32_t*, size_t)
- platformIsExternalReset()
- platformResetCode()
- platformResetRegisterDump(char*, size_t)
- platformResetExceptionInfo(char*, size_t)
- platformRestartDHCP()
- platformSerialHasOverrun(HardwareSerial&)
- platformSerialHasRxError(HardwareSerial&)

**Signature differences (implementation varies, signatures match):**
All signatures match. Implementation differences are expected (e.g. ESP8266 uses wifi_station_dhcpc_stop/start, ESP32 uses esp_netif API).

**Shared-code calls without guards:**
All platform function calls in shared .ino files use functions present in both headers. No guarded-only calls found.

**AC5 — No parity gap found; no audit-fix task required.**
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Audit PASS. Both platform headers contain identical sets of 29 inline platform_* functions with matching signatures. All platform functions used in shared .ino files are present in both ESP32 and ESP8266 headers. No parity gap found, no fix task required.
<!-- SECTION:FINAL_SUMMARY:END -->
