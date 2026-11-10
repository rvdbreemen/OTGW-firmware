---
id: TASK-1782
title: Release v2.0.0-beta.170 public beta (notes over beta.168-170)
status: Done
assignee: []
created_date: '2026-11-10T15:05:00Z'
updated_date: '2026-11-10T15:25:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 878000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public beta of beta.170 (2dda513da) plus the docs and two Greek strings (0b835ccc0, 75ed5ac29) on origin/dev, slot 16:21 on 2026-11-10 (12 h after beta.167). Bundles TASK-1763 (b) mitigation, TASK-1775, TASK-1777. Run via /beta-release-2x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release commit with notes, CHANGELOG rollover, README links; lint clean
- [x] #2 Soak of both bench units read and recorded (bootcount, crashlog, lastreset, MQTT, heap)
- [x] #3 esp32-combo build from the release commit with both SUCCESS lines, under the commit-charge limit
- [x] #4 Tag pushed, prerelease published with --latest=false, assets equal build/, download SHA matches
- [x] #5 #beta-testing text to 97 and posted
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: release commit d260e6965 on df5d32025 (notes over beta.168-170 + two later Greek strings, CHANGELOG [2.0.0-beta.170], README 3 links + notes label); lint clean; evaluate --quick 86/0. AC2: soak: .63 beta.170+2dda513 since 15:21, at 16:23 uptime 1:01, bootcount 5 (= flash+2), MQTT on, heap 54.9 KB, crashlog only the 06:43 beta.167 entry (decoded, TASK-1763); .61 beta.170+749ba83, uptime 0:29, bootcount 284 (80's planned reboots, lastreset Software reset), crashlog empty. AC3: commit charge 78.2 %; build.bat 16:18: 2x SUCCESS + 'Build completed successfully', all assets +d260e69 (80's e93a8c07e landed on top during the freeze and is NOT in the release). AC4: tag v2.0.0-beta.170 -> d260e6965; published 16:23 (after the 12 h slot and the 1 h .63 soak), prerelease --latest=false, Latest v1.7.9; 7 assets = build/; download sha256 5751d556...68c2 = SHA256SUMS. AC5: #beta-testing post by 97, msg 1569734442929365154, read back identical. Not verified: this release build flashed on a bench unit (same source as 2dda513 + docs + 2 Greek strings in i18n/el.json).
<!-- SECTION:FINAL_SUMMARY:END -->
