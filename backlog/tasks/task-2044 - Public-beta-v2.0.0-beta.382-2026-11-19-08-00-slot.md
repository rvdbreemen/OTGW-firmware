---
id: TASK-2044
title: Public beta v2.0.0-beta.382 (2026-11-19 08:00 slot)
status: Done
assignee: []
created_date: '2026-11-19T05:50:00Z'
updated_date: '2026-11-19T07:08:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1049000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Release tracking for the 08:00 public beta slot of 2026-11-19 (/beta-release-2x, standing permission; coordinator GO). Candidate beta.382 (firmware e562f826e, origin/dev 79b9e308e). Covers beta.375-382 since public beta.374: TASK-2025, TASK-1890, TASK-1887, TASK-2018, TASK-1889, TASK-2039, TASK-2040, TASK-2036. Release commit docs-only (notes, CHANGELOG rollover, README); the release build gets its own 1 h soak on .63 (no waiver, coordinator 2026-11-19).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit (notes RELEASE_NOTES_2.0.0-beta.382.md, CHANGELOG [2.0.0-beta.382], README) pushed to dev
- [x] #2 Release build from the release commit soaked 1 h on .63 by otgw-bench-tester: bootcount stable, MQTT connected, no crashlog, heap no collapse
- [x] #3 Tag v2.0.0-beta.382 on the release commit; GitHub prerelease with --latest=false and the 6 assets + SHA256SUMS; download sha256 check; latest stays the 1.x stable
- [x] #4 #beta-testing announcement (all changes since beta.374) and #alpha-testing note posted and read back
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Published v2.0.0-beta.382 on 2026-11-19 07:07:53 UTC (08:07 local): https://github.com/rvdbreemen/OTGW-firmware/releases/tag/v2.0.0-beta.382. AC1: release commit 224e30cec (notes RELEASE_NOTES_2.0.0-beta.382.md, CHANGELOG [2.0.0-beta.382] written from the commits since no Unreleased entries existed, README links) pushed to dev; docs-only (0 src files). AC2: build 2.0.0-beta.382+224e30c (BUILD END 06:00:56 UTC, 1x 'Successfully created ESP32S3 image' + 'Build completed successfully'); otgw-bench-tester release soak on .63 07:03:34-08:04:34 local = PASS: 62/62 device/info 200, bootcount 235 flat (flash +2), 0 panic/loop-stall, MQTT 62/62, freeheap 34.6-35.7k flat, hd_min 25,576->24,944, settings.ini diff 0; core dump present = older poc.10 dump, none from 382. AC3: tag v2.0.0-beta.382 = 224e30cec; prerelease with --latest=false, 6 assets + SHA256SUMS, sizes equal build/, sha256sum -c OK locally and on the downloaded release zip; releases/latest = v1.7.9; body trimmed to 101,648 chars (limit 125,000) with an Earlier changes link and a Coming next line (Kd 0.05, entries 425/427). AC4: #beta-testing 1572871081469943901 / 1572871090622046290 / 1572871099228618814, #alpha-testing note 1572871107730608193 (links the alpha zip post 1572809347149340732), progress update 1572871127498489908 (Robert's request; schedule wording per Robert DM 1572865101592272957); all read back identical. Not verified: SAT control against a real boiler on the release build (no boiler on the bench).
<!-- SECTION:FINAL_SUMMARY:END -->
