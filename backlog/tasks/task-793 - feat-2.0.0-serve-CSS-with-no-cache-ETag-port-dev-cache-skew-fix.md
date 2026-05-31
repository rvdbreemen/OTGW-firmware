---
id: TASK-793
title: 'feat-2.0.0: serve CSS with no-cache + ETag (port dev cache-skew fix)'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-05-31 21:18'
updated_date: '2026-05-31 21:18'
labels:
  - webui
  - caching
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Port dev aa2c8c13. 2.0.0 serves components.css and ds-tokens.css via the onNotFound/handleFile static fallback with NO Cache-Control header, so reflashed CSS can be masked by browser heuristic caching. Add a serveCssRevalidated() helper (no-cache + ETag=getFilesystemHash, If-None-Match -> 304) mirroring the index.html policy in startWebserver(), and register explicit httpServer.on() handlers for the two CSS files. Leave the JS ?v= versioned handlers (lines 199/221) untouched. Note: 2.0.0 has no max-age CSS handler (unlike dev) so this is parity + deterministic revalidation, not a literal 24h-stale fix.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 serveCssRevalidated() helper added: no-cache + ETag(fsHash), If-None-Match returns 304, streams text/css otherwise
- [ ] #2 Explicit httpServer.on() handlers registered for components.css and ds-tokens.css
- [ ] #3 JS ?v= versioned handlers left intact (only CSS handlers added/changed)
- [ ] #4 python build.py exits 0 with per-env SUCCESS verified (esp8266+esp32, fw+fs) and evaluate.py --quick clean
- [ ] #5 On device: one hard refresh after flashing evicts any stale CSS; thereafter CSS revalidates (304 unchanged, fresh after flash)
<!-- AC:END -->
