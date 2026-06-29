---
id: TASK-940
title: >-
  Fix SoftAP AP_FALLBACK passphrase too short (otgw123 < 8 chars strands device
  off-WiFi)
status: Done
assignee:
  - '@claude'
created_date: '2026-06-27 18:27'
updated_date: '2026-06-29 21:52'
labels: []
dependencies: []
ordinal: 153000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On OTGW32 ESP32-S3 (2.0.0 alpha.279), when WiFi STA loses a previously-provisioned connection the WIFI_AP_FALLBACK path calls WiFi.softAP(apSSID, "otgw123") at src/OTGW-firmware/networkStuff.ino:285/91. "otgw123" is 7 chars, below the WPA2 8-char minimum, so esp_wifi rejects it: '[E][AP.cpp:218] create(): passphrase too short!'. The fallback AP never starts, so a device that drops STA is fully unreachable (no STA, no AP) until a manual full-erase reflash. Observed live in Zutphen 2026-06-27 after sustained WS-realism load wedged WiFi. NOTE: the first-boot WiFiManager provisioning portal (startConfigPortal, networkStuff.ino:215) is OPEN and works; only the post-provisioning AP_FALLBACK is broken. See buglog bug-137.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Fallback SoftAP passphrase is >=8 chars (e.g. otgw1234) OR the fallback AP is opened with no passphrase; AP_FALLBACK starts successfully (no 'passphrase too short')
- [x] #2 A device that loses STA after provisioning is reachable on the fallback AP at 192.168.4.1 within the retry window
- [x] #3 Build green esp32/esp32-classic/esp32-combo; evaluate.py --quick no new failures
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
CLOSE 2026-06-29: maintainer verified the device falls back to SoftAP at 192.168.4.1 after losing STA (#2). Passphrase otgw1234 (>=8 chars) confirmed. Closed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SoftAP AP_FALLBACK reachable at 192.168.4.1 after STA loss; passphrase length fixed (otgw1234).
<!-- SECTION:FINAL_SUMMARY:END -->
