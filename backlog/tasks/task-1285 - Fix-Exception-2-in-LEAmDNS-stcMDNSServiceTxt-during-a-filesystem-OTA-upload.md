---
id: TASK-1285
title: 'Fix: Exception (2) in LEAmDNS stcMDNSServiceTxt during a filesystem OTA upload'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-21T20:55:00Z'
updated_date: '2026-10-22T05:55:00Z'
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
- [x] #1 The crash is reproduced on the bench or its allocation path is identified with evidence (heap at the time, the failing allocation)
- [x] #2 The failing allocation is null-safe; a filesystem OTA upload completes without an exception in repeated runs on the bench
- [x] #3 esp8266 build and evaluate.py --quick green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 evidence (scratchpad t1285/mdns_txt_flood.py, mdns_probe.py, fsota_flood.sh):
- Path: core 2.7.4 operator new returns NULL (abi.cpp:36); only runtime site of the default-arg stcMDNSServiceTxt ctor is _readRRAnswerTXT (LEAmDNS_Transfer.cpp:635), reached from _parseQuery known answers on EVERY received query (Control.cpp:318) and from _parseResponse. Helpers.cpp:608 (addServiceTxt, unused by firmware) and Structs.cpp:329 (copy assignment) not reached.
- Without extra heap pressure 300 items per query parse fine (heap 18 KB).
- Repro: fs OTA (100 KB/s) + 300-item queries every 100 ms. Old b266cb6: 2/2 http=000 at 9.7/9.9 s, Exception (2) epc1=0x402357c7 excvaddr=0, addr2line = stcMDNSServiceTxt ctor LEAmDNS_Structs.cpp:60 (same as field). Run 2 lost settings.ini (MQTT user/password gone on the bench).
- Fix b80f098 (build.py nothrow patch at Transfer.cpp:635): 3/3 fs OTA http=200, reboot reason [OTA] Rebooting.
- build.bat Build completed successfully; evaluate --quick 0 failures. Commit c8dd60af1.
- Side finding: bench had two hosts named OTGW (.68 1.x and .61 OTGW32), mDNS conflict; .61 renamed to OTGW2 on request, 2.0.0 default changed in TASK-1293 (dev tree).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A filesystem OTA no longer crashes in LEAmDNS when the heap runs low.

Cause: core 2.7.4 new returns NULL; _readRRAnswerTXT ran the stcMDNSServiceTxt constructor on NULL (Exception (2), LEAmDNS_Structs.cpp:60). LEAmDNS reads the known-answer TXT records of every received mDNS query, and a filesystem OTA lowers the heap. A crash mid-upload can lose settings.ini.

Fix: build.py patches that allocation to new (std::nothrow), next to its existing six RRAnswer patches; the answer is dropped instead.

Tests: bench fs OTA under a 300-item TXT query flood: old 2/2 crash at the field address, new 3/3 complete. Build and evaluator green.

Risk: the patch edits the gitignored core copy on every build; the Makefile path does not apply it (same as the existing RRAnswer patch).
<!-- SECTION:FINAL_SUMMARY:END -->
