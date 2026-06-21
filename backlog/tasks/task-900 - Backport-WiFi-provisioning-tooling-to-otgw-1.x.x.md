---
id: TASK-900
title: Backport WiFi provisioning tooling to otgw-1.x.x
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-21 23:16'
updated_date: '2026-06-21 23:20'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port the host-side WiFi provisioning helpers from dev (2.0.0) to the otgw-1.x.x maintenance line so the ESP8266 can be flashed + re-provisioned headlessly after a USB erase (which wipes WiFi creds from NVS). Scripts are firmware-agnostic (drive the WiFiManager /wifisave portal at 192.168.4.1, present on both ESP8266 and ESP32). Also harden the 1.x .gitignore so credential files can never be committed; secrets must stay in the out-of-repo store %LOCALAPPDATA%/OTGW-capture/.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 bin/provision-wifi-ap.py present on otgw-1.x.x and runs --dry-run without error
- [x] #2 bin/wifi-profiles.py present and 'list' runs without error
- [x] #3 scripts/capture-settings.example.json present as a template with no real credentials (Hardware adapted to esp8266)
- [x] #4 .gitignore on otgw-1.x.x ignores capture-settings.json and *.secret(s).json so credentials cannot be committed
- [x] #5 No real secret file (capture-settings.json / wifi-profiles.json) exists anywhere in the worktree; secrets resolve only from %LOCALAPPDATA%/OTGW-capture or env vars
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Ported provision-wifi-ap.py + wifi-profiles.py byte-identical from dev (both cross-platform, drive WiFiManager /wifisave at 192.168.4.1 which exists on ESP8266+ESP32; no logic adaptation needed). capture-settings.example.json template with Hardware=esp8266. Hardened .gitignore (capture-settings.json, wifi-profiles.json, *.secret(s).json, .env). Verified: py_compile OK, --help + list run, find shows NO real secret file in worktree, git check-ignore confirms creds files are ignored.
<!-- SECTION:NOTES:END -->
