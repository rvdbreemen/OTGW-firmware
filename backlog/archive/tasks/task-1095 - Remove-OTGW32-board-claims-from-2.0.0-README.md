---
id: TASK-1095
title: Remove OTGW32 board claims from 2.0.0 README
status: Done
assignee:
  - '@claude'
created_date: '2026-08-31 16:46'
updated_date: '2026-08-31 16:49'
labels: []
dependencies: []
ordinal: 265000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2.0.0 README markets the OTGW32 board as if it were an available product. It is not for sale. README must describe the ESP32 OpenTherm variant as a prototype, possibly available in the future.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 README no longer presents OTGW32 as a purchasable/available NodoShop board
- [x] #2 ESP32 direct-OpenTherm variant described as prototype, not for sale, possibly future
- [x] #3 Build/feature facts (esp32 env, OTDirect, W5500, BLE) stay accurate
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Follow-up: README now names the ESP32-S3 (LOLIN S3 Mini) in the traditional OTGW with PIC as THE supported hardware. The prototype ESP32 board gets two short mentions (intro, hardware section) and no feature marketing: prototype feature list removed, platform table collapsed to the supported column.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
README no longer names the OTGW32 board. The ESP32 variant with onboard OpenTherm is now described as a prototype that is not for sale at this time and may become available in the future. Build instructions and feature descriptions (esp32 env, OTDirect, W5500, BLE, OLED) are unchanged in substance.
<!-- SECTION:FINAL_SUMMARY:END -->
