---
id: TASK-792
title: 'Fix stale-CSS cache-skew: serve CSS with no-cache + ETag instead of max-age'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 20:57'
updated_date: '2026-05-31 21:02'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Web UI CSS edits never appear after reflash because /index.css is served with Cache-Control: public, max-age=86400 and no revalidation/cache-bust (FSexplorer.ino), while index.html is no-cache+ETag. The browser serves the cached CSS for 24h ignoring reflashes (new HTML text shows, old CSS styling persists). Other CSS (index_dark/index_common/ds-tokens) fall through with no cache headers (heuristic caching). Fix: serve all CSS with no-cache + ETag(filesystem hash) + If-None-Match 304, mirroring the index.html policy, via a shared helper. No HTML/CSS change needed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 /index.css served with Cache-Control: no-cache + ETag (filesystem hash), 304 on If-None-Match match, fresh 200 after a reflash (hash changed)
- [x] #2 index_dark.css, index_common.css, ds-tokens.css served with the same no-cache + ETag policy (no longer heuristic-cached)
- [x] #3 No change to index.css/index_dark.css/index.html content; the existing styling simply reaches the browser
- [x] #4 python build.py exits 0 and python evaluate.py --quick shows no new failures
- [ ] #5 On-device after one hard refresh: boiler-unsupported panel renders with panel background, left-aligned headers and padding; DevTools shows index.css 200 then 304 on revalidation
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
FSexplorer.ino: serveCssRevalidated() helper (no-cache + ETag=fsHash + 304) registered for index.css, index_dark.css, index_common.css, ds-tokens.css. Build --firmware exit 0, evaluate --quick 0 failures. Committed aa2c8c13, pushed origin/dev. AC#5 needs on-device hard-refresh verification.
<!-- SECTION:NOTES:END -->
