---
id: TASK-813
title: >-
  fix(network): restore 1.x.x captive-portal AP SSID naming (full MAC with
  colons) on 2.0.0
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 17:10'
updated_date: '2026-06-02 17:10'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On 2.0.0 the WiFi config portal AP SSID is OTGW-<last3MACbytes> (e.g. OTGW-21B4F8). On 1.x.x it was OTGW-<full 6-byte MAC with colons> (e.g. OTGW-A4:CF:12:21:B4:F8). Maintainer wants the 1.x.x scheme adopted. The 2.0.0 eFuse-MAC source must be kept (WiFi.macAddress() returns zeros before begin() on ESP32); only the formatting changes to full 6 bytes with colons.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Config portal AP SSID is hostname + '-' + full 6-byte MAC formatted with colons (OTGW-AA:BB:CC:DD:EE:FF)
- [ ] #2 MAC still sourced from eFuse via platformGetMacAddress (no revert to WiFi.macAddress())
- [ ] #3 Build green (python build.py --firmware) and evaluate.py --quick shows no new failures
- [ ] #4 Prerelease tag bumped per versioning policy
<!-- AC:END -->
