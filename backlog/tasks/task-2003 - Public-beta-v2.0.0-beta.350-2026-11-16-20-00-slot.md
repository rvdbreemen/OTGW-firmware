---
id: TASK-2003
title: Public beta v2.0.0-beta.351 (2026-11-16 20:00 slot)
status: Done
assignee: []
created_date: '2026-11-16T13:19:00Z'
updated_date: '2026-11-16T19:29:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1025000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 20:00 slot via /beta-release-2x (Robert's standing permission 2026-11-14). Candidate beta.350 (9011c3c48, soak on .63 PASS 13:14-14:15), or beta.351 with TASK-1942 if its soak passes before 19:00 (coordinator). Release commit docs-only: notes for beta.340-350 with per item host/bench evidence, CHANGELOG rollover (lines to add for TASK-1804 A6, 1887, 1888, 1983), README. Composed by comms session 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit pushed at T-75 with notes listing every change since beta.339, on a soaked candidate
- [x] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart
- [x] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [x] #4 #beta-testing post with every change since beta.339 (numbered parts < 2000), read back; #alpha-testing note
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.351 published 2026-11-16 19:06:30Z (20:06 local), prerelease, --latest=false: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.351. AC1: release commit 3f5824201 (docs-only: RELEASE_NOTES_2.0.0-beta.351.md, CHANGELOG [Unreleased] -> [2.0.0-beta.351] with the missing lines for TASK-1942/1887/1888/1804 A6/1983, README beta.351, this task) on top of b95bc21ec (beta.351, TASK-1942); every TASK id in the src commits v2.0.0-beta.339..b95bc21ec has a CHANGELOG line (1699/1996 referenced only). AC2: build from 3f5824201 in wt-comms: 2x 'Successfully created ESP32S3 image', 'Build completed successfully', artifacts 19:09-19:10; ino.bin sha256 0593e580...3318, littlefs b574b1ab...ea81, flash zip 8cae6a24...ef16. Soak by 09 on .63 (OTGW Classic, PIC 6.8, no boiler/thermostat, satsimulation false): flash 19:18-19:19:40, PASS over the full hour to 20:21:01: bootcount 178 (= flash + 2) in 60/60 samples, no crash log, MQTT connected 60/60, freeheap 46.6 -> 45.7 KB flat from minute 10, maxblock 31,732, 0 real panic hits (evidence 09 scratchpad rel351/). DEVIATION: published at 45 of 60 soak minutes on the maintainer's instruction ('Do the public beta release now', relayed 20:05), then he asked to wait for the soak; the body said '45 minutes ... full hour checked afterwards' until the PASS, then '1 h'; the #beta-testing post was held until the PASS. The build started late (18:54) and the flash request went out ~10 min after the build end (reaped watcher). AC3: tag v2.0.0-beta.351 -> 3f5824201; draft then publish; isDraft false, isPrerelease true, releases/latest v1.7.9; download sha256 of the flash zip = SHA256SUMS; 7 assets incl. .elf, safeboot, SHA256SUMS, sizes = build/. AC4: #beta-testing 1571969022822453353 / 1571969029613289726 / 1571969035489382471 (read back intact; every CHANGELOG task id in it); #alpha-testing zip post 1571965231402590290 (worktree at the tag, covers TASK-1942 beta.351; sent before the hold). Not verified: none of the beta.340-351 SAT changes ran on a real boiler in the soak; indigo_light's beta.350 capture confirms PWM entry on a real Vitodens.
<!-- SECTION:FINAL_SUMMARY:END -->
