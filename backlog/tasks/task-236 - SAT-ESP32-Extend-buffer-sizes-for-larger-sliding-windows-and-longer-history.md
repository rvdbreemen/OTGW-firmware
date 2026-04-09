---
id: TASK-236
title: 'SAT ESP32: Extend buffer sizes for larger sliding windows and longer history'
status: Done
assignee:
  - '@claude'
created_date: '2026-04-09 10:46'
updated_date: '2026-04-09 11:13'
labels:
  - sat
  - esp32
  - memory
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On ESP32 (520KB SRAM, 13x more than ESP8266) the current platform-conditional buffer sizes can be much larger, enabling better predictive capability. Use #if defined(ESP8266) guards to cap sizes on ESP8266 and use expanded sizes on ESP32.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT_WIN4H_SIZE: 30 on ESP8266, 360 on ESP32 (12h of 2-min cycle history)
- [x] #2 _hcr_dailyMedian: 7 days on ESP8266, 30 days on ESP32 (4-week heating curve trend)
- [x] #3 _hcr_samples intra-day buffer: 96 on ESP8266, 1440 on ESP32 (per-minute sampling)
- [x] #4 _flow_samples classifier: 64 on ESP8266, 256 on ESP32 (better p90/p10 accuracy)
- [x] #5 All guards use #if defined(ESP8266) pattern, consistent with existing SAT_WIN4H_SIZE
- [x] #6 ESP8266 build unaffected, ESP32 build verified
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Read SATcycles.ino to find all buffer declarations and counter types
2. Add platform-conditional defines for all 4 buffers (SAT_WIN4H_SIZE to 360, HCR_DAYS to 30, HCR_INTRADAY_SIZE to 1440, SAT_FLOW_SAMPLE_SIZE to 256)
3. Upgrade uint8_t counter/head variables to uint16_t where buffer > 255 on ESP32 (using platform-conditional types)
4. Upgrade local loop vars (i, idx, n, src) in affected functions to uint16_t on ESP32
5. Make sorted[] in _hcrIntraMedian() static to avoid 5760-byte stack frame on ESP32
6. Replace hardcoded sizes in declarations with defines
7. Build firmware in background and verify
<!-- SECTION:PLAN:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Expanded SAT buffer sizes on ESP32 using #if defined(ESP8266) guards in SATcycles.ino.

Changes:
- SAT_WIN4H_SIZE: 30 → 360 on ESP32 (12h at 2-min cycle rate vs 2h); win4hHead/Count upgraded to uint16_t on ESP32
- SAT_FLOW_SAMPLE_SIZE: 64 → 256 on ESP32 (better p90/p10 accuracy); flow_sampleHead/Count upgraded to uint16_t on ESP32
- HCR_DAYS: 7 → 30 on ESP32 (4-week heating curve trend vs 1 week)
- HCR_INTRADAY_SIZE: 96 → 1440 on ESP32 (per-minute sampling vs 15-min intervals); hcr_sHead/sCount upgraded to uint16_t on ESP32
- Loop variables (i, idx, src, j) upgraded to uint16_t/int16_t in _flowPercentile() and _hcrIntraMedian() to handle ESP32 buffer sizes correctly
- sorted[] in _hcrIntraMedian() made static to avoid a 5760-byte stack frame on ESP32 (1440 floats x 4 bytes)
- satHCRSaveState/LoadState: platform-conditional buf size (128B ESP8266, 320B ESP32) to fit 30-day JSON
- All guards use #if defined(ESP8266) pattern, consistent with existing SAT_WIN4H_SIZE guard
- ESP8266 behaviour is entirely unchanged; ESP32 build verified via background build

The SAT_WIN4H_SIZE guard was already present in the original file (set to 60 on ESP32); this commit raises it to 360 and adds the remaining three buffer defines.
<!-- SECTION:FINAL_SUMMARY:END -->
