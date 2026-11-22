---
id: TASK-3029
title: 'Public beta v2.0.0-beta.418: release notes, CHANGELOG rollover, README'
status: Done
assignee:
  - '@claude'
created_date: '2026-11-22T07:08:00Z'
updated_date: '2026-11-22T08:27:00Z'
labels:
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1140000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-22 08:05 (coordinator session): 'Do Public beta release.' Candidate origin/dev e57a0b958, firmware 2.0.0-beta.418 (6a2412656). Previous public beta v2.0.0-beta.410. Release commit docs-only (normal slot release, no bump). Firmware tasks in range: TASK-3000 (411), TASK-2957 (412), TASK-2959 (413/414), TASK-3014 (415), TASK-3011 + TASK-3015 (416), TASK-3017 (417/418). Left out of the user-facing list (internal only): TASK-3020 (host test harness portability), bench scripts [no-task], the SAT decision register move (0c59f2db0, docs).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Release notes file with a 'What changed since beta.410' section listing every user-visible change with TASK id, no em dash, OTGW32 not named
- [x] #2 CHANGELOG [Unreleased] rolled over to [2.0.0-beta.418] - 2026-11-22 with compare links
- [x] #3 README disclaimer and breaking-changes link point to beta.418
- [x] #4 #beta-testing post drafted (numbered parts < 2000 chars) and handed to the coordinator
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
BlueFlame 2026-11-22: notes file docs/releases/RELEASE_NOTES_2.0.0-beta.418.md (from beta.410's file; new top section + disclaimer bullet for 411-418; em-dash grep 0, TODO grep 0, OTGW32 not named), CHANGELOG Unreleased -> [2.0.0-beta.418] - 2026-11-22 with Added/Changed/Fixed lines for 3011/3015/3017/2957/2959/3014 plus the existing 3000 line, compare links updated; README disclaimer, breaking-changes link and BLE highlight updated.

Release commit a2cbf9da5 pushed (e57a0b958..a2cbf9da5). #beta-testing draft in OTGW-field-data/_comms/beta418_p1.txt (1683 chars) and beta418_p2.txt (935 chars, soak placeholder), handed to the coordinator; not posted by me. Publish/tag/soak are the coordinator's (skill Phase 3-6).

Follow-up (coordinator, from 09's .63 upgrade settings diff): the one-time BLE failover reset (SATblefailover true -> false, settingStuff.ino:738 at a2cbf9d; entries 455/457/458) added to the upgrade line and as a new 'New from beta.410 to beta.418' block under BREAKING changes; same line in the #beta-testing draft p1. Tag stays on a2cbf9da5.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
v2.0.0-beta.418 published 2026-11-22 08:26 UTC as a GitHub prerelease (latest=false; gh release view: isDraft false, isPrerelease true; releases/latest stays v1.7.10). Tag on release commit a2cbf9da5 (parent = candidate e57a0b958, docs only, firmware 2.0.0-beta.418, 6a2412656). Release body trimmed by the coordinator from the corrected notes file at ec173df25 (106 KB, under the 125000 limit); zip download sha256 4896eacd... equals SHA256SUMS. AC1: notes docs/releases/RELEASE_NOTES_2.0.0-beta.418.md with 'What changed since beta.410' (TASK-3000/2957/2959/3014/3011/3015/3017 by beta number), tested/not-tested bullet, BREAKING block 'New from beta.410 to beta.418' incl. the one-time BLE failover reset (follow-up ec173df25 after 09's .63 settings diff: SATblefailover true->false, settingStuff.ino:738); em-dash 0, TODO 0, OTGW32 not named; internal-only TASK-3020, bench scripts and the register move counted in one line. AC2: CHANGELOG [2.0.0-beta.418] - 2026-11-22 with compare links (a2cbf9da5). AC3: README disclaimer, breaking-changes link and BLE highlight (a2cbf9da5). AC4: #beta-testing post drafted by me (2 parts < 2000 chars) and posted by the coordinator: 1573977976645623868 (1/2), 1573977984384110593 (2/2); #alpha-testing note 1573977988591132803. Soak (otgw-bench-tester, .63 OTGW Classic + S3 Mini, OTGW-field-data/bench-63/2026-11-22_beta418-a2cbf9d-soak/): 08:23:21-09:25:20 UTC, 122 info samples, fwversion 2.0.0-beta.418+a2cbf9d throughout, bootcount 368 constant (no restart), uptime 00:03 -> 01:04, crash log not available (none), MQTT connected, free heap last 34960 B (min 26004, max 35912; steady ~34.5 KB per the coordinator). Build note (coordinator): 1 'Successfully created ESP32S3 image' line (warm safeboot/bootloader cache), 0 src changes after the build. The next beta's change list starts after #beta-testing 1573977976645623868.
<!-- SECTION:FINAL_SUMMARY:END -->
