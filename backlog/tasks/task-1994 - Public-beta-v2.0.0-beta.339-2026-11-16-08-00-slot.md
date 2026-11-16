---
id: TASK-1994
title: Public beta v2.0.0-beta.339 (2026-11-16 08:00 slot)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-16T05:12:00Z'
updated_date: '2026-11-16T07:02:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1020000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta for the 08:00 slot via /beta-release-2x (Robert's standing permission 2026-11-14). Candidate: origin/dev HEAD at T-75 (06:45), firmware beta.339 (92fc8fec7) unless newer lands. Release commit is docs-only: notes, CHANGELOG rollover (with the missing lines for TASK-1919, 1931, 1932, 1972, 1986, 1988 and the TASK-1980 line moved out of the beta.323 section), README. Composed by comms session 92.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit pushed at T-75 with notes listing every change since beta.323, TASK-1986 and TASK-1988 ancestors of it
- [x] #2 Release build SUCCESS, sha256 recorded, 1 h soak on .63 without restart (bootcount = flash + 2, no crashlog)
- [x] #3 Tag on the release commit, draft then publish with --prerelease --latest=false, releases/latest stays v1.7.9, download sha256 equals the build
- [x] #4 #beta-testing post with every change since beta.323 (numbered parts < 2000), read back; #alpha-testing note; gruijter notified about the capture changes
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Prep 06:00-06:30 (92): notes docs/releases/RELEASE_NOTES_2.0.0-beta.339.md built from the beta.323 file: new section 'What changed since beta.323' (16 builds beta.324-339 + capture tool TASK-1972/1986/1988), disclaimer bullet, BREAKING 'New from beta.323 to beta.339', Known issues (TASK-1981 issue removed as fixed; added 1979 HA screenshot/Classic, 1990 no real burner, 1867 no pressure sensor, 1919 no old-vs-fix bench). Lint: no TODO/em dash/relative links/OTGW32; 116887 B < 125000 (body fits). CHANGELOG: [Unreleased] rolled to [2.0.0-beta.339] - 2026-11-16; lines added for TASK-1919, 1931, 1932, 1972, 1986, 1988; TASK-1980 line moved from the beta.323 section (commit 9918e9b88 is after the tag); TASK-1867 line amended with entry 372. Cross-check vs git log v2.0.0-beta.323..origin/dev -- src/ scripts/: all 16 task ids present. README -> beta.339.

Host-suite evidence for the release HEAD (e255ab20b src): 92 swept 115 harnesses that compile SAT/settings code in 5 foreground chunks (113 PASS; test_sat_pm25_poll passes alone, 16 OK); ba ran test_sat_lowload_contract on bbe1cc838 (same src) in 5 foreground chunks: all 18 LL ids PASS (LL-016 skip by design). test_sat_pid_resets fixed in ba861f1cd (red C3861 on the old harness, 17 OK with the stub).

AC1: release commit bd7a133c2 pushed 06:21 on e255ab20b (firmware beta.339 92fc8fec7); git merge-base --is-ancestor f3436717a (TASK-1988) and 526c5e044 (TASK-1986) bd7a133c2: both exit 0. Build bd7a133c2 06:21-06:34: esp32-combo SUCCESS 00:10:46 + fs SUCCESS 00:00:26, 'Build completed successfully!'; app size 2139376 of 2293760 B (93.3 %). sha256 ino bf311162e95dcd97ffc51a4ee3f97f59c3866b9a6f1838a4573defcac184bc02, littlefs f8adb977aef2c9c921f0c6a0098d77ad043b22c5c8f8aa80c637b9c4594204a0, zip 9dd12c5395f91c050a0cba2a3eebcf00b1d4bf63f74d6f52cd113b73977bce1d, merged-full 98bc6788651021d734fa5b90a676045a040b88a1fa0c093a10184b1fd537480a, elf f7f1ffc4738b50f81dfc32217b2cf69ec388c6b2fb50136e737521b84027a949, safeboot ca18634d3a8b1398701edfe45eb08149b401f1aba32e49a47c35c975ebe6f1d8; archive OTGW-build-archive/beta.339-release-92. Sent to 09 for the 1919 check + 1 h soak on .63. beta.339 zip already posted as alpha (1571743145467707473, published-posts.tsv): Phase 6.1 = note, no post script. Internal only, not in the notes: TASK-1993 (build.py logs the app size).

Soak (09, .63 Classic PIC 6.8): OTA 06:35-06:37 app + fs OK, sha256 of ino/littlefs/zip match, device reports 2.0.0-beta.339+bd7a133, settings.ini and /api/v2/settings byte-identical to the pre-flash copy, no crash fields. Bootcount 147 -> 149 (2 OTA restarts) -> 150 (esptool reset of the TASK-1919 otadata dump at 06:38:56: [0] seq=1 VALID | [1] seq=1 VALID). Soak start 06:39:48, baseline bootcount 150, ends ~07:40; verdict from 09 ~07:41.

Published 08:01:44: https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.339 (tag v2.0.0-beta.339 -> bd7a133c2, created 07:42 after 09's soak PASS and the coordinator's GO). gh release view: isDraft false, isPrerelease true; releases/latest = v1.7.9; the draft's 6 asset sizes equal build/; download of the flash zip sha256 9dd12c5395f91c050a0cba2a3eebcf00b1d4bf63f74d6f52cd113b73977bce1d = build. Soak PASS (09): .63 Classic PIC 6.8, 06:39:48-07:40:20, bootcount 150 in 60/60 samples, no crash fields, MQTT 60/60, heap avg 45180 -> 44942, 0 panic/WDT in 17399 telnet lines, settings byte-identical; SAT active on .63.

Posts: #beta-testing 4 parts 1571782384507027516, 1571782390207094856, 1571782395634647204, 1571782401426722939 (read back, lengths 1655/1714/1640/844 = drafts, no OTGW32); every task id of the CHANGELOG section in the post (16) plus TASK-1993 counted as internal. #alpha-testing note + gruijter notice (capture own topics, smaller upload) 1571782467059449866, mention gruijter only; zip already posted as alpha beta.339 (1571743145467707473), no post script (Phase 6.1).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.339 published as the 08:00 public beta. AC1: release commit bd7a133c2 (docs-only on firmware beta.339 92fc8fec7) pushed 06:21, TASK-1986 526c5e044 and TASK-1988 f3436717a ancestors (merge-base exit 0); notes list every change since beta.323, all 16 git-log task ids in the CHANGELOG [2.0.0-beta.339] section. AC2: build SUCCESS (esp32-combo + fs, app 93.3 % of app0), sha256 recorded; 09's 1 h soak on .63 PASS (bootcount 150 stable, no crash, MQTT 60/60, heap flat). AC3: tag on bd7a133c2, draft then publish --prerelease --latest=false, latest stays v1.7.9, download sha256 = build. AC4: #beta-testing 4 parts + #alpha-testing note with the gruijter notice, all read back. Not verified: the Home Assistant card on a real HA screen and the OTGW Classic, the SAT changes with a real burner, TASK-1919 old-vs-fix bench (all named as known issues).
<!-- SECTION:FINAL_SUMMARY:END -->
