---
id: TASK-1057
title: >-
  fix(1.x): close the two ADR-172/173 review defects on otgw-1.x.x (telegraf
  rate-limit bypass, 429 phase-lock starvation)
status: Done
assignee:
  - '@claude'
created_date: '2026-08-03 16:58'
updated_date: '2026-10-01 16:23'
labels: []
dependencies: []
ordinal: 252000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sibling of TASK-1037 (2.0.0/dev, merged via PR #673, e39f737e). That port fixed two defects the adversarial review of 1.7.2-beta.4 found; both still stand on the otgw-1.x.x line.

Defect A - telegraf bypasses the rate limit. restAPI.ino:442 routes /api/v2/otgw/telegraf and /api/v2/otgw/otmonitor to the same handler with the same payload, but kRateLimitedRoutes[] (restAPI.ino:856) lists only { otgw, otmonitor } and { device, time }. There is no kSubTelegraf. A client that switches to the telegraf path polls unlimited, defeating the cap that ADR-086 put in place.

Defect B - 429 phase-lock starvation. checkApiRateLimit() is a single lastServedMs window per route, burst 1. index.js handles 429 by skipping the cycle (index.js:3298, 4202) but never re-phases: there is no random offset anywhere in the poll paths. Two dashboards that land in the same window refuse each other every cycle and freeze silently.

Reference implementation on dev (do NOT cherry-pick, the lines diverge): GCRA limiter with a route->budget INDEX table so aliases share one counter, burst 2, retry_after repeated in the RFC 9457 body because Retry-After is not CORS-safelisted. See restAPI.ino:2474-2510 on dev and ADR-172/ADR-173.

Note the .ino prototype-generator constraint that bit the dev port: pass the budget index (uint8_t), not a reference to a type defined in restAPI.ino, or the hoisted prototype names an undeclared type and the definition collides with it.

Needs its own worktree: wt-otgw-1.x.x already exists at D:/Users/Robert/Documents/GitHub/RvdB/wt-otgw-1.x.x.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 telegraf and otmonitor share ONE rate-limit budget on otgw-1.x.x; exhausting one returns 429 on the other
- [x] #2 Limiter allows burst >= 2 so a Telegraf scrape alongside one open dashboard is not starved; sustained rate stays 1 per window
- [x] #3 429 body carries retry_after in the RFC 9457 payload as well as the Retry-After header
- [x] #4 index.js re-phases at a random offset inside its period on 429, so two dashboards cannot phase-lock; verified with two tabs for 10 minutes
- [x] #5 ADR on the otgw-1.x.x line records the alias-budget and re-phase decisions (own numbering, cross-references dev ADR-172/173)
- [x] #6 python build.py --target esp8266 green, python evaluate.py exit 0, tests/test_evaluate.py green in the wt-otgw-1.x.x worktree
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-09-30 (dev session): the 1.x line already fixed both defects, under its own task numbering, and shipped them in v1.7.5:
- Defect A: 5580e223b (2026-08-25) 'Share one rate-limit budget between the otmonitor and telegraf paths (TASK-1090)'. origin/otgw-1.x.x restAPI.ino: one kRateLimitedRoutes row { otgw, otmonitor, alias telegraf, 1500 ms } with a single lastServedMs.
- Defect B: 053ac9f58 (2026-09-04) 'fix(webui): re-phase polling after a 429 so a second dashboard is not starved'; index.js rephaseDelayMs()/rephaseOTmonitorPolling().
AC status against origin/otgw-1.x.x 8ba6f7ef9 (code read, not run): #1 met (shared budget); #2 NOT met (the 1.x limiter is single-window, 'at most 1 request per window', no burst 2); #3 NOT met (Retry-After header only, no retry_after in the problem+json body); #4 code present, the two-tab 10-minute check is not recorded; #5 NOT met (no 1.x ADR mentions the alias budget or the re-phase; git grep telegraf docs/adr finds only ADR-042); #6 not re-run here.
The 1.x session chose the simpler design (shared single-window budget plus client re-phase) instead of porting dev's GCRA burst 2 and the retry_after body. Maintainer decision: close this task as done differently on 1.x (accept burst 1 plus re-phase, optionally with a short 1.x ADR), or port #2, #3 and #5 in a 1.x worktree. Not changed from this session: the 1.x line may have its own active session.

2026-10-01: decision reversed, burst 2 ported to otgw-1.x.x.

Why the decision was reversed
- On 2026-09-30 my option text said a Telegraf scrape beside an open dashboard would meet a 429 occasionally under the 1.x burst-1 limiter.
- Simulated against the real 1.x checkApiRateLimit(), a 10 s scrape got 0.6% of its scrapes answered, and 750 of 1000 phases got none in 10 minutes. The pair settles in the refused phase.
- The maintainer then chose to port the burst of 2 (2026-10-01).

Where the work landed
- On the 1.x line as its own task, 1.x TASK-1188 (the 1.x numbering; task file on otgw-1.x.x).
- Commit b822ae313 (code, docs, ADR) plus the line's backlog auto-commits.
- Pushed to origin/otgw-1.x.x, 8ba6f7ef9..5fd8c5281.
- What changed:
  - GCRA budget with a burst of 2 and a sustained rate of 1 per window on otmonitor/telegraf (one shared budget, aliasSub) and device/time;
  - retry_after in the 429 problem+json body;
  - index.js re-phase comment updated;
  - CHANGELOG and docs/api/README.md (Server-Side Limits) updated;
  - 1.x ADR-098, Accepted by the maintainer 2026-10-01, amends 1.x ADR-086.

Evidence (full detail in the 1.x TASK-1188 notes; OLD = origin/otgw-1.x.x 8ba6f7ef9)
- AC#1: host harness on the sliced real limiter, A1/A2: exhausting either path returns 429 on the other. Mutant 'alias as a second row' fails exactly A1, A2, T3.
- AC#2: harness T1 (Telegraf 500 ms after a dashboard poll is served), T2 (burst exactly 2), T4 (flood 41 instead of 40 per 60 s). Simulation: Telegraf every 10 s beside one dashboard 0.6% -> 100%, every 60 s 3.4% -> 100%. Browser run with the real index.js: Telegraf 2/60 -> 60/60 beside one tab.
- AC#3: harness J1 (retry_after equal to Retry-After, parsed as JSON) and J2 (273 bytes at the largest values, buffer 320). Mutant 'wrong retry_after' fails exactly J1.
- AC#4: two headless Chrome tabs, 10 minutes, against a host process answering with the sliced firmware limiter. Both tabs served; otmonitor longest gaps 8.9 and 8.0 s (burst 1: 75.8 and 47.6 s). The re-phase itself shipped on 1.x in 053ac9f58; three dashboards without it leave the third with zero grants (simulation, 300/300 runs, under both limiters).
- AC#5: 1.x ADR-098 records the alias budget, the re-phase, the burst of 2 and retry_after, and cross-references dev ADR-172/173. adr-lint strict 0 FAIL; Accepted via adr accept --confirm after the maintainer's yes.
- AC#6: build.bat on the 1.x tree printed "Build completed successfully!" (exit 0), with fresh 1.7.7-beta.2+c453576 .ino.bin/.littlefs.bin/.elf at 07:48. evaluate.py exit 0 after the commit, 47 passed / 1 pre-existing warning, the same as origin.

The maintainer's choice named the burst only; two calls were mine
- device/time also gets the burst of 2: same period < 2 windows arithmetic, one code path, matches dev.
- retry_after in the body (AC#3 as written): 1.x copies any Origin into Access-Control-Allow-Origin, so cross-origin pages are supported clients and cannot read Retry-After.

AC#6 deviations
- There is no build.py --target on the 1.x line (ESP8266 only, arduino-cli), so build.bat ran instead.
- The gates ran in the temporary worktree wt-1057-adr (otgw-1.x.x at the pushed commit), not in wt-otgw-1.x.x, which stayed untouched in case a 1.x session uses it.
- tests/test_evaluate.py has never existed on the 1.x line, so evaluate.py exit 0 against the identical origin baseline stands in for it.

Other notes
- Version: no prerelease bump on 1.x (its TASK-669 rule: the bump is release prep); the change ships in the open 1.7.7-beta.2.
- dev ADR-172 Related Decisions now names the 1.x fixes and the 1.x ADR by title, to avoid an index edge to dev's unrelated ADR-098.
- The two-tab runs recorded earlier in this task used a JS model of the limiter; the oracle runs above replace them as AC#4 evidence.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Closes the two ADR-172/173 review defects on otgw-1.x.x and ports the burst of 2.

The telegraf bypass and the client re-phase had already shipped on 1.x under its TASK-1090 (5580e223b, 053ac9f58). This task added, as 1.x TASK-1188 (commit b822ae313, origin/otgw-1.x.x at 5fd8c5281), a GCRA budget with a burst of 2 on both limited routes and retry_after in the 429 body. 1.x ADR-098 (Accepted 2026-10-01) records it and amends 1.x ADR-086.

Evidence:
- AC#1: harness A1/A2 plus the alias mutant.
- AC#2: harness T1/T2/T4; simulation 0.6% -> 100%; browser, Telegraf 2/60 -> 60/60.
- AC#3: harness J1/J2 plus the retry_after mutant.
- AC#4: two tabs for 10 minutes, longest gaps 8.9/8.0 s (burst 1: 75.8/47.6 s).
- AC#5: ADR-098 Accepted, lint 0 FAIL.
- AC#6: build.bat green (1.7.7-beta.2+c453576), evaluate.py exit 0, equal to the origin baseline.

Deviations on AC#6, all recorded in the notes:
- build.bat ran instead of build.py --target, which 1.x does not have;
- the gates ran in worktree wt-1057-adr, not in wt-otgw-1.x.x;
- tests/test_evaluate.py does not exist on 1.x.

Agent calls within the maintainer's choice: a burst of 2 for device/time as well, and retry_after in the body.

Dev ADR-172's cross-reference is updated.
<!-- SECTION:FINAL_SUMMARY:END -->
