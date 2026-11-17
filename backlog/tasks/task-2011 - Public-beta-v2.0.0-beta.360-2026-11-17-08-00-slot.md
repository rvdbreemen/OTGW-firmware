---
id: TASK-2011
title: Public beta v2.0.0-beta.360 (2026-11-17 08:00 slot)
status: Done
assignee: []
created_date: '2026-11-17T05:15:00Z'
updated_date: '2026-11-17T06:41:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1030000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 08:00 slot via /beta-release-2x. Candidate beta.360 = af92cd8cc (TASK-1887 band: restart below target + 0.1 after a 600 s minimum, fix of the 359 ADR-252 regression), alpha post 1572112371298865192, bench OK on .63. Release commit docs-only: RELEASE_NOTES_2.0.0-beta.360.md (beta.352-360), CHANGELOG rollover (lines added for TASK-1887, TASK-1989), README. Notes > 125000 B: the release body is the trimmed form of Phase 4.4. Composed by comms 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit pushed (coordinator go) with notes listing every change since beta.351, on the benched candidate
- [x] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart (full hour before publish)
- [x] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [x] #4 #beta-testing post with every change since beta.351 (numbered parts < 2000), read back; #alpha-testing note or zip post per the ledger
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.360 published 2026-11-17 06:38:33Z (07:38 local), prerelease, --latest=false: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.360. AC1: release commit 966546795 (docs-only: RELEASE_NOTES_2.0.0-beta.360.md for beta.352-360, CHANGELOG [Unreleased] -> [2.0.0-beta.360] with lines added for TASK-1887 and TASK-1989, README beta.360, this task) on the benched af92cd8cc, pushed 06:18 after the coordinator's go; every TASK id in the src commits since v2.0.0-beta.351 has a CHANGELOG line. AC2: build from 966546795 (commit charge 79.9 % after stopping the Ollama models, detached build with foreground waits): 2x 'Successfully created ESP32S3 image', 'Build completed successfully', 06:31-06:32; ino.bin sha256 baa5b3b0...b297, littlefs be738ddc...162e, flash zip 6f1b6543...b259. Soak by 09 on .63 (OTGW Classic, PIC 6.8, no boiler/thermostat, SAT enabled not active, satsimulation false at start and end): flash 06:33:43-06:35:14, PASS over the FULL hour to 07:36:11: bootcount 186 (= flash + 2) in 60/60 samples, no crash log, MQTT 60/60, freeheap 38.1 -> 37.4 KB flat, maxblock 26,612 constant, 0 real panic hits (09 scratchpad rel360/). Published after the verdict. AC3: tag v2.0.0-beta.360 -> 966546795; draft with 7 assets, re-downloaded from the draft and sha256sum -c 6/6 OK before publishing; isDraft false, isPrerelease true, releases/latest v1.7.9; download sha256 of the flash zip = SHA256SUMS. Body = trimmed Phase 4.4 form (97,173 chars; notes file 126,571 B). AC4: #beta-testing 1572138979497746454 / 1572138985940193316 (read back intact, every CHANGELOG task id in it); #alpha-testing note 1572138994987176008 (key already in the ledger: ba's zip post 1572112371298865192). Not verified: the band change of beta.359/360 has not run with a real boiler (bench without a boiler).
<!-- SECTION:FINAL_SUMMARY:END -->
