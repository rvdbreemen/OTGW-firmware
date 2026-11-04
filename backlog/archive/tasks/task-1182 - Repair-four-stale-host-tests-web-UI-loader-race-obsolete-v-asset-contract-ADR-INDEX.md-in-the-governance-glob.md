---
id: TASK-1182
title: >-
  Repair four stale host tests: web UI loader race, obsolete ?v= asset contract,
  ADR-INDEX.md in the governance glob
status: Done
assignee:
  - '@claude'
created_date: '2026-09-30 11:28'
updated_date: '2026-09-30 11:36'
labels:
  - tests
  - webui
dependencies: []
priority: medium
ordinal: 305000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
DEFECT (dev, test suites only; no firmware change)
A full offline host-suite run on 2026-09-30 (HEAD 4019d4f3a, compared with a clean origin/dev ef84b5980 checkout) found four suites red on BOTH trees, so they predate this session's commits:
- tests/webui/sat-settings-layout.test.mjs: flaky, passed 1 of 4 runs on HEAD and 1 of 4 on origin/dev. It measures computed styles right after domcontentloaded, but since cce5c7433 (2026-07-05, TASK-960 sequential loader) components.css arrives through an async loadCss() chain, so the measurement races the stylesheet (max-width 'none' instead of '100%').
- tests/webui/sat-log-filter.test.mjs: deterministic 'ReferenceError: parseLogLine is not defined'. parseLogLine exists (index.js:2388) but index.js now loads after both stylesheets, and the test calls it at domcontentloaded.
- tests/test_webui_asset_versioning.py: asserts the retired server-side ?v= rewriting (static <link href> tags rewritten by sendIndex). ADR-139 as amended by TASK-958 replaced that with stable URLs plus ETag validation via serveVersionedAsset(); the CSS moved into the index.html loader.
- tests/test_adr_governance.py: load_all() globs docs/adr/ADR-*.md, which matches ADR-INDEX.md (added dd5a70153, 2026-08-07); parse_adr() then crashes on the missing number.
The playwright tests import 'playwright', which is not in the repo (no package.json); they drive system Chrome (channel 'chrome') and read assets from OTGW_DATA_DIR.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 sat-settings-layout.test.mjs waits until components.css is applied before measuring; 10 consecutive runs against the HEAD assets all pass (baseline 1 of 4)
- [x] #2 sat-log-filter.test.mjs waits until index.js has defined parseLogLine before evaluating; it passes against the HEAD assets (baseline: deterministic ReferenceError)
- [x] #3 test_webui_asset_versioning.py asserts the current contract: every stylesheet and script the index.html loader requests has a serveVersionedAsset route with the matching MIME type and is requested without a ?v= query; it passes on HEAD and fails when one route is removed (mutation check)
- [x] #4 scripts/adr_governance.py load_all() reads numbered ADR files only, so tests/test_adr_governance.py runs its live-tree tests instead of crashing; any genuine drift they then report is recorded in this task
- [x] #5 tests/README.md states how to run the playwright web UI tests (playwright package reachable from the test file, OTGW_DATA_DIR, system Chrome)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-09-30 (fresh runs after the last edit, assets = HEAD 4019d4f3a working tree, system Chrome via playwright 1.63.0 staged in the scratchpad):
- AC#1: sat-settings-layout 10/10 consecutive passes (baseline: 1/4 on HEAD, 1/4 on origin/dev; single-sample 'green on origin, red on HEAD' was noise, retracted).
- AC#2: sat-log-filter exit 0, 5/5 checks PASS (baseline: deterministic ReferenceError on HEAD and origin/dev).
- AC#3: test_webui_asset_versioning 2/2 OK; mutant (FSexplorer copy without the /graph.js route) fails with '/graph.js needs a serveVersionedAsset() route'.
- AC#4: governance runs 11 tests instead of crashing. Drift it reported: (a) curated docs/adr/README.md lacked ADR-174..179, fixed by adding six entries (index check now OK); (b) ADR-167 flagged by the lint's full-text binding heuristic, left open as a maintainer decision in TASK-1183.
- AC#5: tests/README.md section 'Web UI tests (tests/webui/)'.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Repairs four host tests that were red on origin/dev before this session (found by a full offline host-suite run).
- tests/webui/sat-settings-layout.test.mjs and sat-log-filter.test.mjs raced index.html's sequential asset loader (cce5c7433, TASK-960): they now wait for components.css to be applied, and for index.js to define parseLogLine/updateFilteredBuffer. Layout: 10/10 passes (was 1/4). Log filter: 5/5 checks (was a deterministic ReferenceError).
- tests/test_webui_asset_versioning.py asserted the retired ?v= rewriting; it now checks the ADR-139/TASK-958 contract (every asset the loader requests has a serveVersionedAsset route with its MIME type, stable URL). Green on HEAD; a removed-route mutant is caught.
- scripts/adr_governance.py load_all() globbed ADR-INDEX.md and crashed; it now reads numbered ADRs only. The live checks then found the curated README index missing ADR-174..179 (added) and one lint item on ADR-167 that needs a maintainer decision (TASK-1183), so tests/test_adr_governance.py still reports that single item.
- tests/README.md documents how to run the playwright tests.
No firmware change, no prerelease bump.
<!-- SECTION:FINAL_SUMMARY:END -->
