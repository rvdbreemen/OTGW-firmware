---
id: TASK-853
title: 'fix(combo): WiFi captive portal must run first in boot sequence'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-10 20:25'
updated_date: '2026-06-10 20:40'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field report: combo build does not launch the captive WiFi portal on a fresh flash (no credentials). Earlier diagnosis: device parks in WiFiManager startConfigPortal() but webserver/portal starves; detection and other subsystem init happen before/around WiFi config. Required: boot sequence runs WiFi configuration (captive portal when no creds) before anything else - before PIC/OTDirect detection, MQTT, webserver, sensors.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 On fresh flash (no WiFi creds), combo boots straight into a working captive portal (AP visible, portal page responds)
- [x] #2 WiFi configuration completes before hardware detection / subsystem init runs
- [ ] #3 Normal boot with stored creds unchanged (connects, then continues boot)
- [x] #4 esp32-combo build green
- [ ] #5 FIELD: user confirms portal works on combo hardware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented: hardware-mode resolution block moved AFTER startWiFi() so the captive portal always comes first. Combo closes UART1 (OTGWSerial.end) before the portal to prevent the floating PIC-RX interrupt-storm starvation, re-opens it for the PIC probe afterwards. activeI2cSda/Scl now also consult persisted settings.iBoardMode (helpers moved below the settings instantiation - combo build error fix). Builds green: esp32-combo + esp8266; evaluate.py --quick 0 failed. ACs 1/3/5 are field-validation.
<!-- SECTION:NOTES:END -->
