---
id: TASK-1877
title: >-
  Classic: device/info showed 'no pic found' after a 203->217 OTA (bootcount
  13); transient boot-probe miss?
status: To Do
assignee: []
created_date: '2026-11-12T21:15:00Z'
updated_date: '2026-11-14T21:19:00Z'
labels:
  - bug
  - board-detect
milestone: 2.0.0
dependencies: []
ordinal: 951000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench .63 (OTGW Classic with S3 Mini, PIC 6.8), 2026-11-12, otgw-bench-tester 09's repro of TASK-1774: after the 19:13 203->217 OTA the device/info read showed "fwversion":"2.0.0-beta.217+a7676dc" "picfwversion":"no pic found" at bootcount 13 (log: 09's scratchpad b217-63\63-repro-203.log). Earlier the same session the panicky FS write (19:01:29, TASK-1774) had happened. Later boot on beta.218 (bootcount 15, checked by the coordinator): picfwversion 6.8, gateway mode, MQTT connected, so the PIC was detected again. ADR-201: combo detection persists only a positive PIC verdict and re-probes every boot, so one missed probe gives one boot without the PIC path. Open: was it a probe miss (timing after an OTA reboot), a read taken before the probe finished, or a real effect of the earlier panic? Check the boot detection log of such a boot and the device/info field timing.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-14 22:35 (09, code reading on dev, not yet bench-verified): picfwversion 'no pic found' is not the detection verdict. detectPIC() (OTGW-Core.ino:1053-1078) sets state.pic.bAvailable from find(ETX) after the PIC reset; the version string state.pic.sFwversion keeps its default 'no pic found' (PICtypes.h:35) until the PIC banner reaches the firmware callback applyPICBannerInfo() (OTGW-Core.ino:847) or processOT() (:6307), which happens later, on the PIC task. The 2026-11-12 read was taken seconds after the FS OTA reboot (mqttconnected:false in the same read, scratchpad b217-63/63-repro-203.log step C), so 'read before the banner was processed' fits, while a missed probe would also have shown hardware_mode degraded. That read did not capture a mode or availability field, so it cannot tell the two apart.

Planned bench (09, after the 1666 overnight recording on .63 ends, 2026-11-15): reboot .63 via /ReBoot and poll /api/v2/device/info and /api/v2/health every 200 ms from the first answer for 60 s, logging picfwversion, the hardware/OT mode fields and uptime, 5 reboots. Expected if this explanation holds: the first answers show picfwversion 'no pic found' with the PIC mode already set, then 6.8 within a few seconds; a missed probe shows degraded mode for the whole boot.
<!-- SECTION:NOTES:END -->
