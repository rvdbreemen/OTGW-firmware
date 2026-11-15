---
id: TASK-1982
title: Public beta v2.0.0-beta.323 (2026-11-15 20:00 slot)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T17:06:00Z'
updated_date: '2026-11-15T19:01:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1012000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tracking task for the public beta v2.0.0-beta.323 in the 20:00 slot of 2026-11-15, run by the beta-release-2x skill on Robert's standing permission. Release commit composed by session 92 on origin/dev b28955bc8 (beta.323). Previous public beta: v2.0.0-beta.294 (TASK-1962).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes and CHANGELOG cross-checked: the 26 TASK ids in src/scripts commits v2.0.0-beta.294..b28955bc8 are in the CHANGELOG section, exclusions named with a reason
- [x] #2 Release commit pushed and the tag v2.0.0-beta.323 on that commit
- [x] #3 esp32-combo build of the release commit: 'Successfully created ESP32S3 image' and 'Build completed successfully' in its log
- [x] #4 1 h soak on .63: bootcount = flash + 2, no crashlog, heap stable
- [x] #5 GitHub release drafted then published with --prerelease --latest=false; asset sizes equal build/; SHA256 of a downloaded asset matches
- [x] #6 #beta-testing post (all parts) posted and read back
- [x] #7 #alpha-testing note posted
- [x] #8 gruijter told that the capture now collects device state, graph history and diag recording (TASK-1964)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1 (92, 18:16): CHANGELOG section [2.0.0-beta.323] holds all 26 TASK ids of git log v2.0.0-beta.294..b28955bc8 -- src/ scripts/ build.py platformio.ini (comm -23 empty); 9 lines added in the release commit (1939, 1943, 1947, 1951, 1958, 1964, 1965, 1969, 1970); 0 exclusions. Notes: 24 builds beta.295-323, the numbers 302, 303, 318, 321, 322 are not in this release; 0 em dashes, 0 TODO, 0 OTGW32, 0 relative links. evaluate.py --quick 94 checks 0 failed. Release commit 0fa19cab1 pushed b28955bc8..0fa19cab1 (AC2: tag still open).

AC3 (92): build of 0fa19cab1 from a clean wt-comms tree, build/logs/build-20261115-181801.log: 2x 'Successfully created ESP32S3 image.' and 'Build completed successfully!', artifacts 18:29-18:30, names 2.0.0-beta.323+0fa19ca. sha256 flash.zip 020203175993164e3a0bbcea59f6b5396d9cfecdc998a927e203c8943d214727, ino.bin 14df3299de07f445beff9e8d58e60bb48c122ef5f931acb51ae5ef6738b7cdad, littlefs.bin 118f8b4df20765f4d628e3dbad0ec1724ebe6030b1c9ff343e9a02f9fe36e947. Archived in OTGW-build-archive/beta.323-release-92/ (incl. .elf + log). Soak (AC4) handed to 09 on .63.

AC4 (09, bench-tester, 19:38): release soak 2.0.0-beta.323+0fa19ca on .63 PASS after 60 min. githash 0fa19ca; bootcount 147 = flash+2 (145 before), constant over 61 samples; no crashlog (also checked in the foreground at 19:38); heap_min_free 40612 -> 37664 within 20 min, then flat to 60 min; heap_free 46596-47612; max_alloc 31732 flat; MQTT connected throughout; sha256 of the 3 flashed files matched; settings diff empty. Evidence: 09 scratchpad rel/63/soak.log. Tag v2.0.0-beta.323 -> 0fa19cab1 and the draft (6 assets = build/ sizes, SHA256SUMS) were made at 18:38 (AC2 tag part); publish waits for the 20:00 slot.

Publish (92, 20:00:21): gh release edit v2.0.0-beta.323 --draft=false --prerelease --latest=false; gh release view: draft=false, pre=true, tag v2.0.0-beta.323, publishedAt 2026-11-15T19:00:22Z; releases/latest stays v1.7.9. Tag v2.0.0-beta.323^{} = 0fa19cab1 (pushed 18:38). Asset sizes equal build/ (checked on the draft). Download check in a new empty folder: sha256sum -c against the published SHA256SUMS: flash.zip OK, ino.bin OK.

Posts: #beta-testing 4 parts 1571600907869753486, 1571600914999943361, 1571600921685663896, 1571600928329568276 (read back: identical to the draft, 1870/1529/1077/838 chars, 0 OTGW32). #alpha-testing note 1571601008147177532 (read back). gruijter notice 1571601016179134706 in #alpha-testing (mention resolves to 733401267665109054).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Public beta v2.0.0-beta.323 published at 20:00 on 2026-11-15. AC1: CHANGELOG [2.0.0-beta.323] holds all 26 TASK ids of v2.0.0-beta.294..b28955bc8 (comm -23 empty), 0 exclusions; notes 24 builds beta.295-323 (302, 303, 318, 321, 322 not in it). AC2: release commit 0fa19cab1 pushed, tag v2.0.0-beta.323 on it (rev-parse ^{} = 0fa19cab1). AC3: build log build-20261115-181801.log 2x 'Successfully created ESP32S3 image.' + 'Build completed successfully!'. AC4: 09 soak on .63 60 min PASS (bootcount 147 = flash+2, no crashlog, heap_min_free flat after 20 min, MQTT connected). AC5: published prerelease, latest=false, latest stays v1.7.9, asset sizes = build/, download sha256 OK for zip and ino.bin. AC6: #beta-testing 4 parts read back identical. AC7: #alpha-testing note 1571601008147177532. AC8: gruijter notice 1571601016179134706. Not verified by me: the soak itself (09's evidence in its scratchpad rel/63/soak.log).
<!-- SECTION:FINAL_SUMMARY:END -->
