---
id: TASK-961
title: >-
  Web server (port 80) does not start on the first boot after WiFiManager
  provisioning — recovers on next reboot
status: To Do
assignee: []
created_date: '2026-06-30 22:07'
labels: []
dependencies: []
ordinal: 173000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Observed on bench OTGW32 .39 (alpha.298+02f6470) after the maintainer flashed + provisioned WiFi (SoftAP/WiFiManager, SSID KeepOut2, RSSI -83 dBm). FIRST boot after provisioning: device joins WiFi and is fully alive (telnet port 23 up, OT-Direct processing, MQTT publish logic running, banner 'fs:ok'), but the AsyncWebServer (port 80, incl /ws) is NOT listening — TCP connections to :80 are ACTIVELY REFUSED (RST, not timeout). A plain hard-reset (esptool --after hard-reset, no re-flash) fixed it: after the clean reboot port 80 is OPEN and /api/v2/device/info returns 200. So server.begin() (FSexplorer.ino startWebserver, unconditional) effectively did not bind on the post-provisioning boot. Hypothesis: the AP->STA transition after WiFiManager provisioning leaves port 80 in a bad state (captive-portal server not fully town down, or server.begin runs before the STA netif/IP is ready and the listener never binds), while telnet (separate SimpleTelnet on 23) and the loop task (OT/MQTT) come up fine. Repro needs the provisioning flow (wipe WiFi creds -> SoftAP -> provide creds -> auto-reboot -> check port 80 on that first boot). Impact: after a fresh flash+provision the user cannot reach the Web UI until they manually reboot - confusing 'bricked web UI' report. Note: seen on the 02f64705 build; confirm whether it reproduces on the current HEAD (bfcb10c1).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: wipe WiFi creds, provision via SoftAP, confirm port 80 refused on the first post-provisioning boot (vs OPEN after a plain reboot)
- [ ] #2 Root cause identified: why server.begin()'s listener does not bind after the AP->STA provisioning transition (captive-portal teardown / netif-not-ready ordering)
- [ ] #3 Fix: web server reliably listens on port 80 on the FIRST boot after provisioning (e.g. (re)begin the server after STA IP is up, or tear down the provisioning server cleanly); verified on-device through a real provisioning cycle
<!-- AC:END -->
