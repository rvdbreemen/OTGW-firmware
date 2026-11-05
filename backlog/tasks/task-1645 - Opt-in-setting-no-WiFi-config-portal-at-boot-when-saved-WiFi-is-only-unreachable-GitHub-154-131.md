---
id: TASK-1645
title: >-
  Opt-in setting: no WiFi config portal at boot when saved WiFi is only
  unreachable (GitHub #154 / #131)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T04:09:00Z'
updated_date: '2026-11-05T10:01:00Z'
labels:
  - github
  - security
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 770000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/issues/154 point 1 (and closed #131). Robert 2026-11-05: an opt-in setting, default = today's behaviour. With the setting on and credentials saved, a boot that cannot reach the router does not open the open WiFiManager portal (networkStuff.ino:225-233, 240 s) but keeps retrying in the background; no credentials or the triple reset (ADR-043) still open the portal. Relates to TASK-1455 (skip after supervisor restart).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Persisted bool setting, default false, REST/UI/help like the other settings
- [ ] #2 Setting on + saved creds + router down at boot: no startConfigPortal, no AP, background STA retry (feedWatchDog), OLED/telnet say 'triple reset to reconfigure'; setting off: unchanged
- [x] #3 No creds and the triple reset still open the portal in both modes
- [ ] #4 Host test of the branches red on old / green on fix (reuse the test_wifi_skip_portal harness); bench: AP off at boot -> no SSID in a scan with the setting on, portal with it off
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
6c 2026-11-05 05:50: implemented in wt-6c-1645 on wt-int4 2a9201f7d; patch at C:/Users/rvdbr/AppData/Local/Temp/claude/d9c572fbc7f8e9ee/9ad25eb8-1a77-4659-887b-f3c41994cc95/scratchpad/task-1645.patch. settings.wifi.bNoPortal (file WifiNoPortal, REST wifinoportal, default false): Networktypes.h, settings write/read, GET + knownSettings, settingsHelp.h, v2.js SET_META (network / Wi-Fi). wifiSkipPortalDecide() returns (marked || settings.wifi.bNoPortal) && !wifiConnected && wifiSaved && !forcePortal, so every startWiFi() with the setting on skips both the config portal and the prerelease AP fallback; no saved creds and the triple reset still open the portal. Telnet log line names the setting and 'Triple reset to reconfigure WiFi'; OLED network page shows 'Triple reset to / reconfigure WiFi' while offline with the setting on. Docs: docs/api/README.md, c4-code-settings.md, ADR-211 (Proposed) Related note. Evidence: tests/test_wifi_skip_portal.py extended (NoPortalSetting 6 tests on rel+pre builds, NoPortalSettingWired 2) - old sources 4 FAIL (the 4 behaviour/wiring tests that need the fix), fix 36/36 OK; tests/test_wifi_deadlink.py stub got settings.wifi, 28/28 OK; settings_whitelist OK, telnet_console_setting OK, adr_governance OK; evaluate --quick 89 checks 82 pass 0 fail 0 warn. settings-help 18 KB test still red (TASK-1646, pre-existing). Not verified: OLED text and the background retry on hardware; bench open: AP off at boot -> no OTGW SSID in a scan with the setting on, portal/AP fallback with it off (AC4).

2026-11-05 bench .61 beta.94: (1) first two runs invalid: the CAPsMAN deny failed silently ('place-before=0' on an empty list -> 'no such item'), the unit roamed to a CAPsMAN AP within 6 s (router log DHCP 10:34:23). (2) With a working deny (router: 'rejected, forbidden by access-list' until removal, rejoin 10:55:03) and a forced WlanScan: setting ON -> the config portal was skipped, BUT device/info showed apfallback=true / 'AP Mode' and the scan saw SSID OTGW-21B4F8: the prerelease loopWifi rule (WIFI_CONNECTING, 2 retries -> startAPFallback) ignored wifinoportal. Fix: that rule now requires !settings.wifi.bNoPortal (networkStuff.ino WIFI_CONNECTING prerelease branch). Test: tests/test_wifi_skip_portal.py new test_prerelease_noportal_setting_never_opens_ap_fallback (control ap_gone pre -> ap=1; ap_gone_noportal pre -> ap=0, no restart): old FAIL ('ap=1' with the setting on), fix 37/37 OK. evaluate 0 failed.
<!-- SECTION:NOTES:END -->
