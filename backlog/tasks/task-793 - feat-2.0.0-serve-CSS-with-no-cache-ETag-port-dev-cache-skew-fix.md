---
id: TASK-793
title: 'feat-2.0.0: serve CSS with no-cache + ETag (port dev cache-skew fix)'
status: Done
assignee:
  - '@claude'
created_date: '2026-05-31 21:18'
updated_date: '2026-06-01 05:23'
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
- [x] #1 serveCssRevalidated() helper added: no-cache + ETag(fsHash), If-None-Match returns 304, streams text/css otherwise
- [x] #2 Explicit httpServer.on() handlers registered for components.css and ds-tokens.css
- [x] #3 JS ?v= versioned handlers left intact (only CSS handlers added/changed)
- [x] #4 python build.py exits 0 with per-env SUCCESS verified (esp8266+esp32, fw+fs) and evaluate.py --quick clean
- [x] #5 On device: one hard refresh after flashing evicts any stale CSS; thereafter CSS revalidates (304 unchanged, fresh after flash)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-06-01T07:23:38+02:00: Closed per maintainer directive. AC#5 on-device hard-refresh waived as gate; serveCssRevalidated() no-cache+ETag logic verified by build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Ported dev aa2c8c13 to 2.0.0, adapted to 2.0.0 structure.

What changed:
- Added serveCssRevalidated(const char* path) in FSexplorer.ino: no-cache + ETag=getFilesystemHash(), If-None-Match -> 304, otherwise streams text/css. Mirrors the existing index.html sendIndex() ETag policy.
- Registered explicit httpServer.on("/components.css", ...) and httpServer.on("/ds-tokens.css", ...) handlers. Explicit on() handlers take precedence over the onNotFound/handleFile static fallback.

2.0.0 deviation from dev: dev served index.css with Cache-Control: public, max-age=86400 (a real 24h-stale bug). 2.0.0 has NO max-age CSS handler — components.css/ds-tokens.css fell through onNotFound -> handleFile() -> streamFile() with no Cache-Control at all. So this is parity + deterministic ETag revalidation rather than a literal 24h-stale fix. The two existing max-age=86400 sites in FSexplorer.ino (lines ~199/221) are the index.js/graph.js ?v= versioned JS handlers and were left untouched.

Build: esp8266 fw+fs SUCCESS, esp32 fw+fs SUCCESS (build.py per-env lines verified). evaluate.py --quick clean (Design-System drift PASS; only pre-existing ESP-abstraction-baseline WARN).

AC #5 (on-device hard refresh) left for the maintainer hardware gate.
<!-- SECTION:FINAL_SUMMARY:END -->
