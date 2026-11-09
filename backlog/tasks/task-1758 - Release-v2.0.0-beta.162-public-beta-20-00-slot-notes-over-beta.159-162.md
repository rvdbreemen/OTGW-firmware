---
id: TASK-1758
title: 'Release: v2.0.0-beta.162 public beta (20:00 slot, notes over beta.159-162)'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-09T17:07:00Z'
updated_date: '2026-11-09T20:05:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 858000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Public 2.x beta via /beta-release-2x, coordinator assignment 2026-11-09. Candidate beta.162 (5786928c8), release commit docs-only on origin/dev (src identical). Bundle: TASK-1743, 1741, 1733/ADR-223, 1747; tests 1742, 1739, 1736, 1738; docs 1755.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes, CHANGELOG rollover and README link in one docs-only commit on origin/dev; lint clean (no em dash, no relative link)
- [x] #2 Candidate soak: beta.162 on the bench unit at least 1 h with MQTT, no restart, no crashlog (or a recorded decision)
- [x] #3 esp32-combo build from the release commit: 2x 'Successfully created ESP32S3 image', fresh artifacts with the release githash
- [x] #4 Tag v2.0.0-beta.162 on the release commit; GitHub prerelease with --latest=false; asset sizes equal build/, SHA256 of the downloaded zip equals SHA256SUMS; latest stays v1.7.9
- [x] #5 #alpha-testing zip post and #beta-testing text handed to the comms agent
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Release done 2026-11-09 ~21:10. Commit 338293d1f (docs only; evaluate --quick 85 pass / 0 fail; lint: no TODO/em dash/relative link/board name). Soak: Robert waived the 12:02 boot panic (relayed by the coordinator ~19:00); .61 beta.162 uptime 7:48, bootcount 193 unchanged at 20:2x. First build killed by Claude Code on low memory; rebuild 21:02: 2x 'Successfully created ESP32S3 image', 'Build completed successfully'. Tag v2.0.0-beta.162 -> 338293d1f pushed; prerelease published with --latest=false (Latest stays v1.7.9); 7 assets sizes equal build/; downloaded zip sha256 95a08eb6...e758 equals SHA256SUMS. #alpha-testing zip post REFUSED by discord-post-build.sh ('prerelease 2.0.0-beta.162 was already published'): the beta.162 alpha zip was posted before; not worked around. #beta-testing text sent to session 97 for posting (Discord MCP down here).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: commit 338293d1f (notes, CHANGELOG rollover, README; evaluate --quick 85/0/0; lint clean). AC2: soak gate waived by Robert for the 12:02 boot panic (relayed by the coordinator); .61 ran beta.162 7 h 48 min after it without restart, bootcount 193 unchanged; panic tracked by detectie as TASK-1763. AC3: build.bat esp32-combo 21:02, 2x 'Successfully created ESP32S3 image' + 'Build completed successfully' (first run killed by Claude Code on low memory). AC4: tag v2.0.0-beta.162 -> 338293d1f; prerelease published --latest=false, Latest stays v1.7.9; 7 assets equal build/; download sha256 matches SHA256SUMS. AC5: #alpha-testing zip post refused by the script because the beta.162 alpha zip was already published (covered, not worked around); #beta-testing announcement posted by 97, msg 1569442675374035126, read back. Not verified: the /update browser route with this zip; no bench flash of the 338293d build (same firmware source as 5786928 on .61).
<!-- SECTION:FINAL_SUMMARY:END -->
