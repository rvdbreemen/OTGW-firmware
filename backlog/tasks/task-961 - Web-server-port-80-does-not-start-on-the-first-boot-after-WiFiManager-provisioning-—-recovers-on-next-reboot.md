---
id: TASK-961
title: >-
  Web server (port 80) does not start on the first boot after WiFiManager
  provisioning — recovers on next reboot
status: To Do
assignee: []
created_date: '2026-06-30 22:07'
updated_date: '2026-07-09 21:26'
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

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Reproduced live 2026-07-05 on classic-S3 + PIC 6.6 (alpha.327+827abbc). After WiFiManager provisioning, first boot: device joined LAN (192.168.1.219), ping OK, telnet port 23 OPEN, but HTTP port 80 dead — curl / returned HTTP 000 on 5/5 tries over ~30s. Single hard-reset (esptool --after hard_reset) fixed it: port 80 came up HTTP 200 within 5s of reboot, /api/v2/device/info served normally. Confirms 'web server does not start on first boot after provisioning, recovers on next reboot'. Repro is deterministic here: happens every fresh provision, clears on any reboot.

Independently reproduced 2026-07-06 on Classic-S3 COM8 (192.168.88.64) during TASK-1016 provisioning: first boot after SoftAP provisioning had telnet/OT/MQTT alive but HTTP port 80 actively refused (curl 000). A --update re-flash (forces reboot, preserves WiFi creds) fixed it immediately — HTTP 200 + WS 101 after. Confirms AC#1 repro on current HEAD (alpha.328+f32071c), not just the older 02f64705 build.

2026-07-09 drain review: cannot fully close autonomously — AC#1/#3 require a real provisioning cycle (wipe WiFi creds -> SoftAP -> enter WiFi credentials -> reboot -> check port 80). Entering WiFi credentials is a user/credential action I do not perform, and wiping creds would drop the bench device off the network mid-drain. Kept open for a maintainer-driven provisioning cycle; the fix direction (re-begin the AsyncWebServer after the STA IP is up, or cleanly tear down the captive-portal server) is documented in the description.
<!-- SECTION:NOTES:END -->
