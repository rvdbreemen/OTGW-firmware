---
id: TASK-1026
title: Analysis + bake-in optimum + ADR + bump/commit
status: Done
assignee:
  - '@claude'
created_date: '2026-07-05 22:23'
updated_date: '2026-07-06 04:31'
labels: []
dependencies: []
ordinal: 237000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
TASK-1015. Select final N (robustness-first, latency tiebreak). Bake into REST_MAX_INFLIGHT + WEB_FILE_MAX_INFLIGHT + client MAX_INFLIGHT. Write ADR (method+data+chosen N, supersedes TASK-1014 single-flight rule). Update CLAUDE.md rule to 'at most N'. Bump prerelease + commit.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Final N baked into firmware caps + client
- [x] #2 ADR written via adr-kit + accepted
- [x] #3 CLAUDE.md single-flight rule updated to at-most-N; bump+commit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-165 drafted via adr-kit (docs/adr/ADR-165-optimal-request-parallelism-esp32s3-webui-rest.md), committed a553c0a1c, pushed to origin/dev. Status: Proposed, deliberately NOT Accepted — the study's own design spec (docs/superpowers/specs/2026-07-06-parallelism-loadtest-design.md, Non-goals) requires maintainer sign-off before any production change. AC#1 (bake N into firmware caps + client) is DELIBERATELY NOT DONE: production build_flags defaults (REST_MAX_INFLIGHT=4, WEB_FILE_MAX_INFLIGHT=6 in restAPI.ino) are unchanged, no client-side MAX_INFLIGHT JS knob was added (doesn't exist yet - would be new frontend work), CLAUDE.md's single-flight rule is unchanged. All of TASK-1015's actual measurement work is complete and pushed (TASK-1016/1018/1019/1020/1021/1022/1024 all Done); this task stops here pending @robert's review of ADR-165 and an explicit go-ahead to bake N*=2 into production.

Maintainer (Robert) read ADR-165 in-session and approved N*=2 as the new hard limit, directing the bake-in. Flipped ADR-165 Status: Proposed -> Accepted (2026-07-06), with a proper Status History transition attributed to the maintainer, and rewrote the stale 'does NOT yet authorize' section into a 'Bake-in status (post-acceptance)' section reflecting what was actually done. Baked in: (1) src/OTGW-firmware/restAPI.ino REST_MAX_INFLIGHT 4->2 and WEB_FILE_MAX_INFLIGHT 6->2 (both #define defaults, with updated inline comments referencing ADR-165); (2) CLAUDE.md's 'Single-flight API calls' rule rewritten to document the N<=2 hard cap, citing ADR-165, while explicitly noting the existing frontend serialization pattern (still N=1 in practice) remains correct and unchanged since 1<=2. Deliberately did NOT build a client-side MAX_INFLIGHT JS knob - no such feature was requested, and the current single-flight frontend already satisfies the new N<=2 ceiling. Verified live: rebuilt esp32-classic (no PLATFORMIO_BUILD_FLAGS override this time - the change is now the real #define default), evaluate.py --quick 0 failures/1 pre-existing warning, flashed COM8, confirmed the gate now sheds at offered=4 (12/40 503s, whereas the old default of 4 would have let 4 through untouched) via scripts/loadtest_harness.py. Bumped prerelease (alpha.329->alpha.330) via bin/bump-prerelease.sh and committed everything (ADR acceptance + restAPI.ino + CLAUDE.md + version banners) in one commit. Also updated Claude's own cross-session memory (feedback_single_flight_api_calls.md, new project_adr165_parallelism_n2.md) per the maintainer's explicit request to document this in memory.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
TASK-1015 epic complete end-to-end. ADR-165 Accepted by maintainer sign-off 2026-07-06. N*=2 baked into production: REST_MAX_INFLIGHT and WEB_FILE_MAX_INFLIGHT both lowered from 4/6 to 2 in src/OTGW-firmware/restAPI.ino, CLAUDE.md's single-flight rule updated to document the new N<=2 hard cap. No client-side MAX_INFLIGHT knob built (not requested, not needed - frontend's existing N=1 discipline is within the new cap). Verified live on the esp32-classic bench device (build+evaluate green, flash confirmed the gate now sheds above offered=2). Prerelease bumped to alpha.330 and everything committed/pushed in one commit.
<!-- SECTION:FINAL_SUMMARY:END -->
