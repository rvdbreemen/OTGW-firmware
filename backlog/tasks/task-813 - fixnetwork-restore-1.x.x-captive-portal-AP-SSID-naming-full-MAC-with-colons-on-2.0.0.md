---
id: TASK-813
title: >-
  fix(network): restore 1.x.x captive-portal AP SSID naming (full MAC with
  colons) on 2.0.0
status: Done
assignee:
  - '@claude'
created_date: '2026-06-02 17:10'
updated_date: '2026-06-02 17:16'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On 2.0.0 the WiFi config portal AP SSID is OTGW-<last3MACbytes> (e.g. OTGW-21B4F8). On 1.x.x it was OTGW-<full 6-byte MAC with colons> (e.g. OTGW-A4:CF:12:21:B4:F8). Maintainer wants the 1.x.x scheme adopted. The 2.0.0 eFuse-MAC source must be kept (WiFi.macAddress() returns zeros before begin() on ESP32); only the formatting changes to full 6 bytes with colons.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Config portal AP SSID is hostname + '-' + full 6-byte MAC formatted with colons (OTGW-AA:BB:CC:DD:EE:FF)
- [x] #2 MAC still sourced from eFuse via platformGetMacAddress (no revert to WiFi.macAddress())
- [x] #3 Build green (python build.py --firmware) and evaluate.py --quick shows no new failures
- [x] #4 Prerelease tag bumped per versioning policy
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Changed startWiFi() in networkStuff.ino to build the config-portal AP SSID suffix from the full 6-byte eFuse MAC formatted with colons (OTGW-AA:BB:CC:DD:EE:FF), matching the v1.x.x scheme. Kept platformGetMacAddress() (eFuse) as the source since WiFi.macAddress() returns zeros before WiFi.begin() on ESP32. Shipped as alpha.142; esp32+esp8266 both build clean, evaluator green.
<!-- SECTION:FINAL_SUMMARY:END -->
