---
id: TASK-1188
title: Give the poll rate limiter a burst of 2 and repeat retry_after in the 429 body
status: Done
assignee:
  - '@claude'
created_date: '2026-10-01 05:21'
updated_date: '2026-10-01 16:21'
labels:
  - rest-api
  - rate-limit
dependencies: []
priority: medium
ordinal: 240000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up to TASK-1090 (shared otmonitor/telegraf budget, 429 re-phase in the web UI), tracked on the dev line as its TASK-1057.

The limiter is a single window per route with burst 1. A Telegraf scrape beside one open dashboard is refused whenever it lands inside the 1500 ms window the dashboard's last otmonitor poll opened, which is 75% of the 2000 ms poll period. Telegraf keeps a fixed phase and the dashboard re-phases only when it is refused itself, so the pair drifts into the arrangement where Telegraf is always refused. Modelled against the real checkApiRateLimit() with a 10 s scrape: 0.6% of the scrapes answered, and 750 of 1000 phase offsets got no answer at all in 10 minutes.

Maintainer decision 2026-10-01: port the 2.0.0 line's GCRA limiter with burst 2 and a sustained rate of 1 per window (dev ADR-172), and repeat retry_after in the RFC 9457 body, because Retry-After is not CORS-safelisted and sendCorsOriginHeader() admits cross-origin browser clients.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otmonitor and telegraf keep sharing one budget: exhausting either path returns 429 on the other (host harness on the real code, both directions)
- [x] #2 Both limited routes allow a burst of 2 with a sustained rate of 1 per window; a Telegraf scrape beside one open dashboard is served (host harness, simulation, browser run)
- [x] #3 The 429 problem+json body carries retry_after equal to the Retry-After header and parses as JSON at the largest values (host harness)
- [x] #4 Two dashboards opened together are both served for 10 minutes against the real limiter (browser run)
- [x] #5 An ADR records the decision, amends ADR-086 and cross-references the 2.0.0 line's ADR-172 and ADR-173
- [x] #6 build.bat green with its SUCCESS line and fresh binaries, python evaluate.py exit 0, tests/test_evaluate.py green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-01. FIX = this change's working tree; OLD = origin/otgw-1.x.x 8ba6f7ef9 (burst 1). The harness, simulation and browser scripts live outside the repo (session scratchpad); their method and results are recorded here and in ADR-098.

AC#1, #2, #3: host harness lim1057
- Method: the real limiter is sliced from restAPI.ino by anchor (OLD via git show), every slice checked verbatim and brace-balanced, then compiled with MSVC and run as 16 cases.
- FIX: 16/16 pass.
- OLD fails exactly the 6 defect cases:
  - T1: Telegraf 500 ms after a dashboard poll (served 1 then 0);
  - T2: burst (1 of 5 served);
  - T6: full burst after 25.5 days idle (1 then 0);
  - D1: device/time burst (1 then 0);
  - Z1: millis()==0 sentinel (5 of 5 served);
  - J1: retry_after missing from the body.
- OLD passes the 10 controls:
  - A1/A2: exhausting either path gives 429 on the other;
  - T3: alternating paths, 7 per 10 s (FIX 8, cap 8);
  - T4: flood, 40 per 60 s (FIX 41);
  - T5: Retry-After 1;
  - R1: across the millis() wrap;
  - C1: window refill;
  - C2: device/time has its own budget;
  - C3: POST never limited;
  - J2: 273-byte body at max values parses as JSON (buffer 320).
- Five mutants each fail exactly their cases:
  - alias as a second row: A1, A2, T3;
  - signed rollover: T6;
  - Retry-After rounded down: T5;
  - burst 1: T1, T2, T6, D1, J1;
  - wrong retry_after: J1.
- RESULT PASS.
- FIX J1 body: retry_after=1 equal to Retry-After=1, detail "...1 request per 2 second(s) (burst 2). Retry after 1 second(s)."

AC#2: simulation sim1057
- Setup: against the same slices; client model from index.js (setInterval; on 429 a restart after U[0,P), first tick one period later); Telegraf at a fixed phase (agent defaults: Interval 10 s, RoundInterval true, no CollectionJitter; telegraf config/config.go NewConfig).
- One dashboard plus Telegraf, 1000 phases x 10 min, Telegraf served (OLD -> FIX):
  - every 10 s: 0.6% -> 100% (phases with no answer: 750 -> 0);
  - every 60 s: 3.4% -> 100%;
  - every 5 s: 0.8% -> 75.8%;
  - the dashboard: 99.7-99.9% -> 99.7-100%.
- Two dashboards in phase, 300 x 120 s: 63/58% -> 76/73% served, mean longest gap 25.8 -> 7.8 s.
- Three dashboards without re-phase: the third gets zero grants in 300/300 runs under both limiters; with the re-phase none.
- Two dashboards plus Telegraf every 10 s: Telegraf 17.5% -> 46.4% (over budget by design).

AC#2, #4: browser runs twotab1057.mjs
- Setup: the real 1.x index.js in headless Chrome, one process per tab, against a host server whose limiter decisions, Retry-After and body come from oracle1057.exe (the sliced firmware limiter); 10 minutes each.
- FIX, one tab plus Telegraf every 10 s: Telegraf 60/60 served (longest gap 10.0 s); tab 300/300.
- OLD, one tab plus Telegraf every 10 s: Telegraf 2/60 (longest gap 588.8 s); tab 297/299.
- FIX, two tabs opened together: otmonitor 191/261 and 205/268 grants, longest gaps 8.9 and 8.0 s; device/time longest gaps 20.6 and 23.4 s.
- OLD, two tabs opened together: 139/244 and 167/256, longest gaps 75.8 and 47.6 s; device/time 43.6 and 84.3 s.
- Every client was served in all four runs.

AC#5: ADR-098
- Gives the poll rate limiter a burst of 2 and repeats retry_after in the 429 body; amends ADR-086 (cross-reference recorded on both sides with adr relate); cross-references the 2.0.0 line's ADR-172/173.
- adr-lint --strict: 0 FAIL; adr-readiness: ready-for-confirmation.
- ADR-098 Accepted by the maintainer in session 2026-10-01 (adr accept --confirm, after the acceptance packet); the pre-commit adr-judge checked 72 ADRs with Enforcement blocks.

AC#6: build.bat and evaluate.py
- build.bat (arduino-cli, ESP8266) exit 0 with "Build completed successfully!".
- Artifacts written 07:48:28-07:48:42, after the 07:45:08 start: OTGW-firmware-1.7.7-beta.2+c453576.ino.bin (769984 B), .littlefs.bin (2072576 B) and .elf.
- No compiler warning in restAPI.ino or index.js. The build rewrote version.h/version.hash (githash, build number); both were restored to HEAD, not committed.
- python evaluate.py exit 0 on the staged change: 46 passed, 2 warnings (the String usages below, plus the notice that the working tree holds uncommitted changes).
- python evaluate.py exit 0 after commit b822ae313: 47 passed, 1 warning (9 String usages, pre-existing). origin/otgw-1.x.x gives the same 47/1/0.

AC#6 deviation: tests/test_evaluate.py does not exist on otgw-1.x.x and never did (git log origin/otgw-1.x.x -- tests/test_evaluate.py is empty). The AC was copied from the 2.0.0 line's TASK-1057 by mistake. Checked on the evidence of evaluate.py exit 0 and the identical origin baseline.

Version: no prerelease bump. On 1.x the bump is release prep (TASK-669); this change ships in the open 1.7.7-beta.2. A beta.3 bump made by the 2.0.0 rule was reverted before the build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Gives the poll rate limiter a GCRA budget with a burst of 2 and a sustained rate of one request per window on both limited routes. otmonitor and telegraf keep sharing one budget (aliasSub), and the 429 problem+json body repeats Retry-After as retry_after for cross-origin pages. ADR-098 (Accepted 2026-10-01) records the decision and amends ADR-086. CHANGELOG [Unreleased] and docs/api/README.md (new Server-Side Limits section; OpenTherm poll recommendation now 10 s or more) are updated, and the index.js re-phase comment describes the burst-2 limiter. Ships in the open 1.7.7-beta.2; no bump on 1.x (TASK-669). Commit b822ae313.

Evidence per AC (details in the notes):
- AC#1: harness A1/A2, exhausting either path returns 429 on the other; mutant 'alias as a second row' fails exactly A1, A2 and T3.
- AC#2: harness T1/T2/T4 (burst exactly 2; flood 41 instead of 40 per minute); simulation, Telegraf every 10 s beside one dashboard 0.6% -> 100%; browser, Telegraf 2/60 -> 60/60 beside one tab.
- AC#3: harness J1/J2, retry_after equal to Retry-After, parsed as JSON, 273 bytes at the largest values; mutant 'wrong retry_after' fails exactly J1.
- AC#4: two browser tabs for 10 minutes against the real limiter, both served, longest gaps 8.9/8.0 s (burst 1: 75.8/47.6 s).
- AC#5: ADR-098 Accepted; adr-lint strict 0 FAIL.
- AC#6: build.bat 'Build completed successfully!', fresh 1.7.7-beta.2+c453576 binaries; evaluate.py exit 0, the same as origin.

Deviation, AC#6: tests/test_evaluate.py does not exist on otgw-1.x.x (copied from the 2.0.0 line by mistake); covered by evaluate.py exit 0 and the identical origin baseline.
<!-- SECTION:FINAL_SUMMARY:END -->
