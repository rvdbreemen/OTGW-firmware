---
id: TASK-1285
title: 'Fix: Exception (2) in LEAmDNS stcMDNSServiceTxt during a filesystem OTA upload'
status: To Do
assignee: []
created_date: '2026-10-21T20:55:00Z'
labels:
  - bug
dependencies: []
priority: medium
ordinal: 251000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench 2026-10-21 22:44:10 (.88.68, 1.7.8-beta.6+9fbafad): a filesystem OTA upload ended in "Exception (2) Access to invalid address", epc1=0x4023576b, excvaddr=0x00000000; curl got http=000. addr2line: esp8266::MDNSImplementation::MDNSResponder::stcMDNSServiceTxt::stcMDNSServiceTxt(char const*, char const*, bool), LEAmDNS_Structs.cpp:60 (core 2.7.4). A null dereference in the mDNS service TXT constructor, most likely a new that returned NULL under low heap during the upload (core 2.7.4 new returns NULL). build.py already patches the RRAnswer allocation in LEAmDNS for the same class of failure; this is a different struct. The router log shows a deauth reason 3 and an immediate reconnect at 22:44:09, i.e. the crash reboot. Earlier filesystem OTAs the same day (20:16, 20:34) completed with http=200, so it is intermittent.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The crash is reproduced on the bench or its allocation path is identified with evidence (heap at the time, the failing allocation)
- [ ] #2 The failing allocation is null-safe; a filesystem OTA upload completes without an exception in repeated runs on the bench
- [ ] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->
