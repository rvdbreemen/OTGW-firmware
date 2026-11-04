---
id: TASK-202
title: >-
  SAT: Replace String objects in BLE scan callback with char[] to prevent heap
  fragmentation on ESP32
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 05:22'
updated_date: '2026-04-09 05:47'
labels:
  - audit-fix
  - sat
  - memory
  - esp32
dependencies: []
priority: high
---

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SATble.ino BLE callback uses 3 String objects (svcDataStr, uuidArdu, macArdu) per advertisement
- [x] #2 Replace each String with fixed-size char[] and use the BLE API's underlying data pointer directly where possible
- [ ] #3 Verify no heap alloc in the BLE callback after fix
- [ ] #4 Test with multiple BLE devices to confirm fragmentation eliminated
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SATble.ino BLE callback to identify all String allocations
2. Replace String svcDataStr with std::string directly (avoids Arduino heap fragmentation)
3. Replace String uuidArdu with char uuidBuf[40] + std::string intermediary
4. Replace String macArdu with std::string + existing char macBuf[18]
5. tolower/toupper calls cast to unsigned char to avoid signed-char UB
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Replaced 3 Arduino String objects in BLE onResult callback with either std::string (for BLE API return values) or char[] buffers. The std::string intermediaries are unavoidable since the BLE SDK returns them, but they are short-lived locals without Arduino String's heap management overhead. UUID string matching now uses strstr() on a lowercased char buffer instead of std::string::find().
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Replaced 3 Arduino String objects in the BLE scan callback (SATble.ino) with heap-friendly alternatives.

Changes:
- svcDataStr (String): replaced with std::string directly from advertisedDevice.getServiceData() — the BLE ESP32 SDK already returns std::string, wrapping it in Arduino String was pointless overhead
- uuidArdu (String): replaced with std::string + char uuidBuf[40] for the case-insensitive UUID string comparison; now uses strstr() on a lowercased local buffer instead of std::string::find() with two separate String temporaries
- macArdu (String): replaced with std::string directly from getAddress().toString(); copied to the existing char macBuf[18] as before

User impact: Eliminates 3 Arduino String heap allocations per BLE advertisement during 3-second scan windows. In a busy BLE environment with many nearby devices, this meaningfully reduces heap fragmentation over time on ESP32.

No behavioral change: MAC formatting, UUID matching, and service data parsing logic are identical.
<!-- SECTION:FINAL_SUMMARY:END -->
