---
id: TASK-1976
title: >-
  Build: an image can report the parent githash in device/info fwversion
  although build.py stamped HEAD
status: Done
assignee:
  - '@claude'
created_date: '2026-11-15T15:02:00Z'
updated_date: '2026-11-15T16:19:00Z'
labels:
  - build
  - tooling
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1007000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen by 09 on the TASK-1931 bench build (2026-11-15): GET /api/v2/device/info fwversion = 2.0.0-beta.318+1233ba9 (the parent commit), while the zip, the .ino.bin name and /version.hash on LittleFS say 56fb6db. The build came from the committed HEAD: reflog commit 15:15:36, amend 15:17:31, BUILD START after; build log (ba scratchpad build-1931.log) line 36 'autoinc-semver.py ... --githash 56fb6db', line 57 'Building version: 2.0.0-beta.318+56fb6db', lines 306 and 870 '[version] Updating version.h (hash=56fb6db)'. The committed version.h held 1233ba9: bin/bump-prerelease.sh stamps the hash of HEAD at bump time, which during a cherry-pick restack is the parent. Other builds today (309+ada14c6, 314) reported the right githash. Open: why the compiled string kept the committed stamp although version.h was rewritten (compile before the restamp? a cached object? which pio step reads it), and whether every build after a restack shows it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduce: a restack (cherry-pick -n + bump + commit) then build.bat; device/info fwversion vs the zip githash
- [x] #2 Find the cause in build.py / autoinc-semver.py / the pio pre-scripts with evidence (log order, object timestamps)
- [x] #3 Fix so fwversion always carries the HEAD the build ran on; a host or build check that fails when they differ
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Cause found (2026-11-15, ba). The 318 ELF itself carries 2.0.0-beta.318+1233ba9 (archive OTGW-build-archive/2.0.0-beta.318+56fb6db.elf, .ino.bin and merged-full.bin all hold it), the 324 ELF holds 2.0.0-beta.324+c1fa2b2 (correct; same restack flow, committed version.h f85e7b1). Not build.py: in both logs the pio pre-script restamp ([version] Updating version.h) runs before 'Converting OTGW-firmware.ino' and the .ino.cpp.o compile. Timeline from the session transcript (UTC): 14:18:47 build-1931.cmd started; 14:29:13 my wait loop hit its 590 s timeout and went to the background while the build kept going; 14:31:27 I ran the post-build step (archive + 'cd .. && git checkout -- src/OTGW-firmware/version.h src/OTGW-firmware/data/version.hash', chained with ';' after a failing ls of the not-yet-existing zip). That put the committed stamp 1233ba9 back on disk. 14:34:35 (file mtime 15:34:35 local) pio compiled OTGW-firmware.ino.cpp.o with that header; firmware.elf 15:37:22. The objects are LTO bytecode, so the stamp cannot be read from the .o directly. A human/agent racing the build, not a tooling bug; the zip name and /version.hash came from build.py's own githash, the app string from the reverted header.

Fix: build.py verify_app_version_string() runs in main() right after rename_build_artifacts(): it reads the renamed *.ino.bin and exits 4 when the exact semver build.py stamped is missing, naming the version strings the image does hold. tests/test_build_app_version_check.py (4 cases: match, stale githash refused, no app image, main() calls it after the rename): on the old build.py FAILED (failures=1, errors=3), on the fix OK. Against the real archived images: 2.0.0-beta.318+56fb6db.ino.bin refused ('it holds: 2.0.0-beta.318+1233ba9'), 2.0.0-beta.324+c1fa2b2.ino.bin accepted. All tests/test_build*.py 9/9; evaluate.py --quick 85 pass, 0 fail. AC1: a restack plus build alone does not reproduce it (the 324 build is a restack, committed version.h f85e7b1, and its image holds +c1fa2b2); the defect reproduces only with version.h reverted mid-build, shown by the archived 318 image and the transcript timeline above. Process side: I wait for EXIT= in the build log before any post-build step in that worktree (memory feedback_builds_foreground_under_10min / this task).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cause (AC2): not the build tooling. During the beta.318 bench build my own post-build step (git checkout -- version.h) ran while pio was still compiling, after my wait loop had timed out to the background; OTGW-firmware.ino.cpp.o was compiled at 15:34:35 from the reverted header (stamp 1233ba9), the artifact names came from build.py's githash 56fb6db. Evidence: the archived 318 ELF/.ino.bin hold 2.0.0-beta.318+1233ba9, transcript timestamps 14:29:13 timeout, 14:31:27 checkout, object mtime 15:34:35 local. AC1: a restack plus build alone is correct (324 image holds +c1fa2b2); it needs the mid-build revert. AC3: build.py verify_app_version_string() after the rename exits 4 on a mismatch; tests/test_build_app_version_check.py red on the old build.py, green on the fix; refuses the real 318 image, accepts the real 324 image; test_build* 9/9, evaluate --quick 0 fail.
<!-- SECTION:FINAL_SUMMARY:END -->
