---
id: TASK-1645
title: >-
  Opt-in setting: no WiFi config portal at boot when saved WiFi is only
  unreachable (GitHub #154 / #131)
status: To Do
assignee: []
created_date: '2026-11-05T04:09:00Z'
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
- [ ] #1 Persisted bool setting, default false, REST/UI/help like the other settings
- [ ] #2 Setting on + saved creds + router down at boot: no startConfigPortal, no AP, background STA retry (feedWatchDog), OLED/telnet say 'triple reset to reconfigure'; setting off: unchanged
- [ ] #3 No creds and the triple reset still open the portal in both modes
- [ ] #4 Host test of the branches red on old / green on fix (reuse the test_wifi_skip_portal harness); bench: AP off at boot -> no SSID in a scan with the setting on, portal with it off
<!-- AC:END -->
