---
id: TASK-1659
title: >-
  Review PR #697: native asset loading + bounded server-side file queue
  (andrebrait, ADR-212 Proposed)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-05T14:37:00Z'
updated_date: '2026-11-06T01:03:00Z'
labels:
  - github
  - pr
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 783000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
https://github.com/rvdbreemen/OTGW-firmware/pull/697 by andrebrait, base dev, +1658/-476, 72 files. Replaces the v2.html script loader with native asset loading plus a bounded server-side file queue (ADR-212 Proposed). His follow-up (comment 6660042343): queue alone partly fixes; queue + JS retry fine up to ~2 tabs but max free block 7668 B with two tabs (below the 8192 guard); a single file (CSS/JS/fonts inlined) has no 503s and the most free heap; gzipped ~1.1-1.4 s vs ~2.9-4.0 s. Architecture choice is Robert's (queue/ADR-212 vs single gzipped file superseding ADR-192; TASK-1640 gzip was parked). Note: the PR reuses TASK-1657, which on dev is the beta.95 release task (number collision).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Robert's architecture decision recorded (queue/ADR-212, single gzipped file, or neither)
- [ ] #2 Reviewer team on the PR head (correctness, project rules/ADRs incl. ADR-139/147/165/188/192, tests); findings posted as a PR review
- [ ] #3 Tests + evaluate + combo build green on the PR head in its own worktree; bench page-load under 2 tabs with heap numbers
- [ ] #4 Merge only on Robert's go
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-05 Robert (session): wait for andrebrait's gzip+ETag measurement, then choose the direction. PR comment posted (thanks, the ADR-192/139 implications, the two-tab 7668 B finding, the TASK-1657 number collision -> use TASK-1659, merge conflict with dev).

2026-11-05 15:52: PR rebased onto dev, single commit 491c20b98 now uses TASK-1659, GitHub reports MERGEABLE. Direction still waits for the gzip+ETag measurement (Robert's decision).

Robert 2026-11-06 ~00:00 (AskUserQuestion, lead session), after Andre's gzip+ETag measurement (PR #697 comment 6670567252: one /v2.html.gz 170,558 B, first load 200+ETag 1.17 s, reload 304 0.13 s, stale ETag 200 full): 'Beide: eerst queue, gzip later'. The request queue of PR #697 (ADR-212) goes first; the single gzipped page is a follow-up with its own ADR (supersedes ADR-192), after measuring today's multi-file load time. Next for 1659: review PR #697 head in its own worktree (tests, evaluate, esp32-combo build, bench), post the review, then ask Robert for the merge itself.

2026-11-06 01:50 inline review pass 1 (lead, read-only, commit charge 85% so no agents/build): PR head 491c20b98, base ffd1405b5 (behind origin/dev: needs rebase, version banners will conflict). Core: webServerCompat.h WebQueuedFileResponse = response-owned bounded FIFO (4 entries, 128 B path, 48 B mime, no open file while waiting), admission via webFileGateTryAdmit (cap 2, 1 below maxblk 16000) plus webFileGateCanSend (ADR-188 TX cap, queued waiters excluded by pcb+owner match in platformTcpTxHoldingOnPort). 10 s deadline answered 503 Retry-After 1 from the _ack poll; RX timeout 11 s while queued. File slot no longer armed in the disconnect callback (g_fileSlotHeld removed); destructor releases slot and pumps the next. Points to check in the gate run: (1) client() null-safety in pump()/error() after a disconnect; (2) iWebfile503Count now counts only queue-full/deadline refusals, so old dashboards see fewer 503s by design; (3) 7 new hd_webfile_* device/info fields (REST contract addition, additive); (4) FSexplorer.png moved from serveStatic to webSendFile (now gated); (5) ADR-212 is Proposed: offer to Robert before merge. Not yet run: tests, evaluate, combo build, bench.

2026-11-06 01:55 review point (1) checked against ESPAsyncWebServer 3.11.2 (.pio/libdeps/esp32/ESPAsyncWebServer/src/WebRequest.cpp): ~AsyncWebServerRequest deletes and nulls _client (l.97-104) BEFORE deleting _response (l.106-109); ~WebQueuedFileResponse dequeues itself before pump(), so pump() only touches the other, still-live queued requests. _onPoll (l.242-246) calls _ack(this,0,0) only with _client non-null and canSend(), so the 10 s deadline path runs with a live client. Point (1) holds: no null client deref found by reading. Not proven at runtime; the bench run (disconnect while queued) must still show it.

2026-11-06 02:10 gates on PR head 491c20b98 in ../wt-pr697: evaluate.py --quick 90 checks, 83 passed, 0 failed, 0 warnings (health 100%). tests/test_asset_loading_acceptance.py 9 passed. tests/test_build_staging.py exit 0. test/host/test_rest_body_stale.py ALL PASS (S5, S5b, S6, S7).

FINDING (review, blocking for the test, not the firmware): tests/test_web_file_scheduling.py cannot compile on the PR head with any compiler. It slices platform_esp32.h from 'struct PlatformTcpIdleConnection {' to '// Exception cause', and on this base that range also holds platformResolveHost (TASK-1605, c469c5166) and platformGatewayProbeSend (TASK-1450, af5f0ecba), which need IPAddress, addrinfo, pbuf and raw_sendto that the fixture does not fake. Compile errors are all in tcp_gate.inc (MSVC run). Likely written before the rebase onto TASK-1450/1605. Fix: end the slice at the next marker after platformTcpTxHoldingOnPort (e.g. '// TASK-1605: resolve').

With that slice end patched in a scratch copy only (scratchpad/gxxshim/sched_patched.py, compiled through MSVC cl via a g++ shim because this PC has no host g++/clang++): PR head 'PASS: production webSendFile scheduling (0 failures)', exit 0. --old-rev ffd1405b5 is red only as a COMPILE failure (fixture uses the new iWebfile* counters), not a behavioural failure on the old code: second finding, the old-vs-new claim in the docstring is weaker than stated.

Third point: the test requires clang++/g++ only (shutil.which), while the repo's other host tests use MSVC via find_vcvars on Windows; on a Windows dev machine it raises 'A host C++ compiler is required'. Not run yet: tests/webui/native-asset-load.test.mjs (headless Chrome, skipped at 86% commit charge), full run_tests.py, esp32-combo build, bench. PR review comment not posted yet: waits until the gates are complete so it is one review.

2026-11-06 03:00 first-part review posted on PR #697 (issuecomment-6673794452): gates green so far, findings 1-3 on tests/test_web_file_scheduling.py (slice end, compile-only old-rev red, MSVC fallback). Remaining: browser test, full run_tests, rebase, combo build, bench, ADR-212 to Robert, merge.
<!-- SECTION:NOTES:END -->
