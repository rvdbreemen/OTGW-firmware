---
id: TASK-870
title: 'fix(security): /pic PIC-flash route is unauthenticated (no checkHttpAuth)'
status: Done
assignee: []
created_date: '2026-06-15 06:06'
updated_date: '2026-06-15 06:48'
labels: []
dependencies: []
ordinal: 86000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found during the TASK-869 team review (adjacent finding). The /pic route (server.on('/pic', HTTP_ANY, upgradepic), FSexplorer.ino:154) maps to upgradepic() (OTGW-Core.ino:5711-5784), which performs PIC firmware flash / refresh / delete based on the 'action' arg. upgradepic() calls webBeginRequest() but NEVER calls checkHttpAuth() anywhere in its body, so it runs with NO Basic Auth and NO CSRF same-origin check even when an HTTP password is configured. By contrast the sibling admin routes /ReBoot and /ResetWireless DO guard with checkHttpAuth (FSexplorer.ino:572,580). Any device on the LAN can flash or delete PIC firmware over HTTP with no credentials; CLAUDE.md warns PIC firmware operations can brick the PIC. This is PRE-EXISTING and independent of the TASK-869 method-mapping fix (that fix does not touch /pic). Fix: add 'if (!checkHttpAuth()) return;' at the top of upgradepic() after webBeginRequest(), and confirm the web UI PIC-flash flow still works (it shares the browser's cached Basic Auth from the page load, ADR-056). Consider also narrowing /pic from HTTP_ANY to the methods it actually needs.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 upgradepic() calls checkHttpAuth() before any PIC flash/refresh/delete side effect
- [ ] #2 Web UI PIC-flash still works (browser sends cached Basic Auth)
- [ ] #3 Build green 3 targets; evaluate.py --quick no new failures
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
WONT-FIX / BY DESIGN. Maintainer confirmed (2026-06-15) that unauthenticated PIC updates are intentional: it is the current security model (trusted LAN). /pic (upgradepic) deliberately does not call checkHttpAuth(). This is an accepted carve-out from ADR-056 for now, not a defect. No code change. Recorded so future code reviews do not re-flag it.
<!-- SECTION:FINAL_SUMMARY:END -->
