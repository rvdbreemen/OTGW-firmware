---
id: TASK-1877
title: >-
  Classic: device/info showed 'no pic found' after a 203->217 OTA (bootcount
  13); transient boot-probe miss?
status: To Do
assignee: []
created_date: '2026-11-12T21:15:00Z'
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
