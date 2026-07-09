---
id: TASK-898
title: >-
  Combo firmware does not serve HTTP/Telnet on PIC-less OTGW32 (boot-detect
  stall)
status: To Do
assignee: []
created_date: '2026-06-21 16:22'
updated_date: '2026-07-09 21:26'
labels: []
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 122000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Field-observed 2026-06-21 on OTGW32 (ESP32-S3, MAC 10:20:ba:21:b4:f8) while recovering it via USB with the esp32-combo build (alpha.235, fixed passive-BLE).

Symptom: after a clean erase_flash + merged-full flash + WiFi provisioning, the device joins WiFi and is STABLE on the network (10/10 ICMP replies on 192.168.1.143, no reboot loop) but BOTH port 80 (web) and port 23 (telnet) refuse connections (HTTP=000, connection refused) consistently. USB-CDC silent. So the LWIP/WiFi stack is up but the application servers never start -> setup() appears to stall before server.begin().

Suspected cause: the combo boot-detect (PIC vs OTDirect, ADR-127) false-detects a PIC on a PIC-less OTGW32 via floating GPIO44 (the 'sticky false-PIC-detect' risk recorded in the combo single-target audit; mitigation tracked as TASK-865.17) and blocks init waiting on a PIC that isn't there. Needs a boot-mode indicator + force/clear OTDirect so the combo proceeds to start the web/telnet servers.

NOT caused by the TASK-895 BLE change (that is passive-only and safe). The same hardware boots + serves fine on the esp32-otgw32 (OTDirect-only) target.

Impact: blocks the maintainer directive to make esp32-combo the single shipped build (the combo cannot recover/serve on a PIC-less OTGW32 until this is fixed).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduce: flash esp32-combo to a PIC-less OTGW32, confirm WiFi up (ping) but port 80/23 refused; capture where setup() stalls (telnet unavailable -> use USB-CDC debug or a temporary Serial trace gated for combo).
- [ ] #2 Root-cause the boot-detect: confirm GPIO44 floating-read false-PIC-detect (or find the real stall point); document the exact code path that blocks before server.begin().
- [ ] #3 Add a boot-mode force/clear (per TASK-865.17): the combo must reach server.begin() and serve web+telnet on a PIC-less OTGW32, with a way to force OTDirect when PIC detection is ambiguous.
- [ ] #4 Field-validate: esp32-combo on a PIC-less OTGW32 serves /api/v2 and telnet; AND on a real Classic-with-PIC it still boot-detects PIC correctly.
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
UPDATE 2026-06-21: did NOT reproduce after a clean erase_flash + fresh esp32-combo merged-full flash + WiFi provisioning. The combo serves HTTP fine (HTTP 200 on /api/v2/sat/ble/discovery) on the KeepOut2 network (device 192.168.88.39). The earlier 'port 80/23 refused' was observed only on the Koekie network on a device that had just been through the active-burst crash + several back-to-back flashes — likely a transient corrupt/half-boot state, not a combo boot-detect stall. Possibly related to task-853 (combo captive-portal boot-sequence ordering). Downgraded to medium; reopen to HIGH only if it recurs on a clean flash.

BENCH EVIDENCE 2026-06-29 (OTGW32 @192.168.88.39, alpha.285, ESP32 build, PIC-less): HTTP /api/v2/health -> 200 AND Telnet :23 OPEN while otgwconnected:false / OT-Direct. The esp32 (OTGW32) build serves both services fine PIC-less. HOWEVER 898 is COMBO-build-specific (runtime boot-detect stall); the combo build has a different partition table, so validating it requires a merged flash that erases NVS -> SoftAP -> user-gated provisioning, which would strand the bench. Not closeable autonomously on a single networked board; needs a combo flash + on-site provisioning.

2026-07-09 drain review: cannot close — AC#1/#4 need a PIC-less OTGW32 board, which is not on the bench/network this session (scanned 192.168.1.143/.88.143/.1.39/.88.39, none reachable). NOTE: the boot-detect that caused the original stall has since been heavily reworked (TASK-947/949/ADR-160 + the TASK-1031 fix landed today: no-PIC now leaves iBoardMode=0 and proceeds to initOTDirect rather than blocking on a phantom PIC), so this may already be fixed — but that must be CONFIRMED on a real PIC-less OTGW32 before closing. Kept open pending that hardware.
<!-- SECTION:NOTES:END -->
