---
id: TASK-1962
title: Public beta v2.0.0-beta.294 (10:00 slot 2026-11-15)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T08:40:00Z'
updated_date: '2026-11-15T08:59:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1003000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Slot release of beta.294+67e3f51 (soaked 1 h on .63, PASS). Docs-only release commit: notes (by 92), CHANGELOG rollover, README. Standing permission (Robert 2026-11-14).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Candidate soaked >= 1 h on .63 (bootcount flash+2, no crashlog, MQTT)
- [x] #2 Release commit built, bench check on .63 OK
- [x] #3 Tag + GitHub prerelease (--latest=false), assets sha match
- [x] #4 #alpha-testing note and #beta-testing announcement posted
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC1: 09 soak .63 beta.294+67e3f51 08:37-09:37, 60 samples 0 failed, bootcount 135 (=flash+2) constant, crashlog none, MQTT connected every sample, heap_min_free 33320->32920 (boot->60 min), max_alloc 31732 flat.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: 09 soak .63 beta.294+67e3f51 08:37-09:37 (60 samples, bootcount 135 = flash+2, no crashlog, MQTT connected, heap_min_free 33320 -> 32920). AC2: release commit bcf067aef (docs-only: CHANGELOG rollover, README; notes 4004f17b7) built with build.bat 09:43-09:49: 'Build completed successfully', 1x 'Successfully created ESP32S3 image' (warm build, bootloader cached), fresh artifacts; evaluate --quick 86/0; 09 OTA on .63: sha256 match, fwversion 2.0.0-beta.294+bcf067a, PIC 6.8, MQTT, settings changed none (151 keys), bootcount 137 = flash+2 stable at 5 min, no crashlog. AC3: tag v2.0.0-beta.294 -> bcf067aef; draft then published 09:57:54 as prerelease --latest=false (latest stays v1.7.9); 7 assets, sizes equal build/; downloaded flash zip sha256 f3191ce2...097b = SHA256SUMS. Body = full notes file (96976 chars, under the 125000 limit). AC4: #alpha-testing note 1571449432208048178 (zip already posted as alpha 1571379615799316540, post ledger hit, no second zip post); #beta-testing announcement 1571449437954383932 (1564 chars, read back). Note: published 7 s before the 12 h mark after beta.250 (21:58:01).
<!-- SECTION:FINAL_SUMMARY:END -->
