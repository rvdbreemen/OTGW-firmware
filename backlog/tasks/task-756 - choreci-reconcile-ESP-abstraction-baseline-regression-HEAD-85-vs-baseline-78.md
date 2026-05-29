---
id: TASK-756
title: >-
  chore(ci): reconcile ESP-abstraction baseline regression (HEAD 85 vs baseline
  78)
status: Done
assignee:
  - '@claude'
created_date: '2026-05-29 09:10'
updated_date: '2026-05-29 09:35'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
evaluate.py ESP_ABSTRACTION_BASELINE=78 but committed HEAD scans 85 platform-conditional sites: a +7 regression slipped in since baseline commit 9be88a0d without updating the ratchet. Investigate which 7 sites are new (diff violation list at HEAD vs 9be88a0d), then either route them through platformXxx() shims / HAS_* flags to bring the count back down, or — only if genuinely necessary — record them as acknowledged debt with justification in the audit doc. Do NOT silently raise the baseline to mask real leaks.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The 7 sites above baseline are identified (file:line) and classified fixable vs necessary
- [x] #2 Fixable sites routed through shims/HAS_* flags; evaluate.py ESP abstraction check no longer FAILs
- [x] #3 Any retained sites documented in docs/audits/2026-05-28-esp-abstraction-leak-audit.md with rationale; baseline reflects reality with justification
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Regression isolated to 2 files: SimpleTelnet.h +4 (independent vendored lib) and settingStuff.ino +3 (TASK-564 crc32/snapshot no-op detection). Resolution: (1) per user, SimpleTelnet excluded from scanner via ESP_ABSTRACTION_EXCLUDED_LIB_DIRS (independent lib, not app code, not modified); (2) the 3 crc32 sites moved behind platformSettingsNoopCapture/Unchanged shims in platform_esp8266.h (crc32) + platform_esp32.h (snapshot+memcmp), preserving each platform's deliberate strategy; coredecls.h include moved into platform_esp8266.h. Count restored to 78 = baseline (no FAIL); baseline unchanged/accurate, not raised. The 2 remaining settingStuff BLE guards are original baseline entries (Tier TASK-740..746 scope). Audit doc section 3.1 added. Bumped alpha.90; build verifying.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Resolved the +7 ESP-abstraction regression (85 vs baseline 78). Isolated to two files: SimpleTelnet.h (+4) and settingStuff.ino (+3). SimpleTelnet is an independent vendored library (own upstream/portability layer, not OTGW app code) and is excluded from the scanner via ESP_ABSTRACTION_EXCLUDED_LIB_DIRS in evaluate.py (library not modified, per maintainer). The 3 settingStuff sites (TASK-564 crc32/snapshot settings no-op detection) were routed through platformSettingsNoopCapture/Unchanged shims in platform_esp8266.h (crc32 sentinel) and platform_esp32.h (full-struct snapshot+memcmp); each platform's deliberate strategy is preserved byte-for-byte, coredecls.h moved into platform_esp8266.h. Count restored to 78=baseline; evaluator ESP check now WARN (not FAIL), Failed:0. Baseline left at 78 (accurate, NOT raised to mask). The 2 remaining settingStuff BLE guards are original baseline entries owned by Tier tasks TASK-740..746. Audit doc section 3.1 added. Build green ESP32+ESP8266 (alpha.90).
<!-- SECTION:FINAL_SUMMARY:END -->
