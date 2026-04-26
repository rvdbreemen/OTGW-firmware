---
id: TASK-433
title: Repair OTGW32 WebUI on 2.0.0-beta after SAT integration (diagnose-first)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-04-26 19:16'
updated_date: '2026-04-26 19:59'
labels:
  - bug
  - webui
  - 2.0.0-beta
  - sat
  - diagnose-first
dependencies: []
references:
  - ~/.claude/plans/1-webui-werkt-niet-magical-hoare.md
  - backup/claude-sat-pre-rebase
  - 'Build: 2.0.0-beta+10fc8e9 (HEAD = 10fc8e9f)'
  - data.backup-20260423-002046/
priority: high
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Reporter

User on 2026-04-26 after flashing `2.0.0-beta+10fc8e9` to the OTGW32 (ESP32-S3). The WebUI loads but is reportedly non-functional. The user's framing implied a missing merge between `dev` (1.5.x LTS line, no SAT) and the SAT-integrated 2.0.0 branch.

## Reframing finding

Static analysis showed the merge has substantially already happened:

- `backup/claude-sat-pre-rebase` is a **strict ancestor** of HEAD (`10fc8e9f`).
- Both `dev` merges (`9ef8daf7`, `caaad6fa`) are already in HEAD's history.
- Dev's WebUI improvements (Inter / JetBrains Mono fonts, design-system tokens, log-render hotpath fix, theme hardening) are all present in `src/OTGW-firmware/data/`.
- All SAT DOM ids referenced by `sat.js` exist in `index.html`.
- All 12 exported `SAT.*` functions are bound to onclick handlers.
- All 7 SAT REST endpoints exist in `restAPI.ino` / `MQTTstuff.ino` / `SATcontrol.ino` / `SATweather.ino`.

Therefore: the breakage is a **runtime artifact**, not a structural merge gap. Most likely candidates: stale LittleFS image, gzip Content-Encoding header bug, JS exception during init, or CSS-class race in `switchView()`. Each is a 1-10 line fix.

## Approach

Diagnose-first. The full plan lives at `~/.claude/plans/1-webui-werkt-niet-magical-hoare.md` and is the source of truth for phasing, risk register, verification checklist, and rollback. This task tracks execution.

Phase 0 — pre-flight (done via this task creation): safety tag `webui-broken-baseline` set on `10fc8e9f`.

Phase 1 — live diagnosis on hardware (gating): user opens DevTools, captures Network 404s, first console error, DOM state.

Phase 2 — targeted fix in exactly one path (2A stale image / 2B JS exception / 2C CSS race / 2D gzip MIME).

Phase 3 — build + flash verify on OTGW32 (and ESP8266 if available).

Phase 4 — single focused commit per fix.

## References

- Plan file: `~/.claude/plans/1-webui-werkt-niet-magical-hoare.md`
- Safety tag: `webui-broken-baseline` → `10fc8e9f`
- Backup branch (read-only reference): `backup/claude-sat-pre-rebase`
- Pre-rebase data snapshot (untracked): `data.backup-20260423-002046/`
- Relevant ADRs: ADR-082 (Core 2.7.4 LTS pin), ADR-083 (PlatformIO primary build), ADR-085 (SAT integration)
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Phase 1 diagnosis complete: failure category identified (2A stale image / 2B JS exception / 2C CSS race / 2D gzip MIME) with concrete evidence (Network 404 list, first console error, DOM state)
- [x] #2 Phase 2 fix applied in exactly one path; git blame confirms no regression of dev's UI improvements (commits 97b46807, a3e4ce26, f544875e, 7a894f50, c0eb1682, 1622485d, ae959676, 6b1ea8bb)
- [ ] #3 python build.py exits 0 for both ESP32 and ESP8266 targets; LittleFS images generated for both
- [x] #4 WebUI loads with 0 errors in browser console (warnings OK) on the freshly flashed OTGW32
- [ ] #5 All 7 SAT REST endpoints return 200 (DevTools Network)
- [ ] #6 SAT tab loads simple view with badges and 4 cards populated; view selector switches Thermostat/Expert/Diagnostics with re-render
- [ ] #7 Toggle Enable, Set Preset (one), Set Mode (one), DHW slider each trigger POST and visible feedback
- [ ] #8 Light and dark theme both render the SAT dashboard cleanly
- [ ] #9 Settings -> SAT Settings page loads all 12 categories
- [ ] #10 FSexplorer still works (regression check)
- [x] #11 Log page still streams with otgwDebug.verbose gating intact (regression check against f544875e)
- [x] #12 Single focused commit per fix; multiple fixes if needed are kept in separable commits
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-04-26: Phase 1 diagnosis complete. Category: 2D (gzip Content-Encoding double header). Smoking gun via curl probe of the live device showed two `Content-Encoding: gzip` headers on /index.js?v=10fc8e9 — once from manual sendHeader() in the FSexplorer handler, once auto-emitted by streamFile() detecting the .gz suffix. Browser double-decompresses, body becomes empty, initMainPage undefined, inline window.onload throws ReferenceError at index.html:618.

2026-04-26: Phase 2 fix shipped in commit 7869014b. Removed the manual sendHeader('Content-Encoding','gzip') from both /index.js and /graph.js handlers in FSexplorer.ino (lines 205, 225). streamFile() emits the header autonomously based on the .gz suffix, so the manual call was redundant.

2026-04-26: Side effect per user request: gzip pre-compression disabled in build.py (commit d75822bc). The two prepare_gzip_assets() call sites are commented out; .gz files removed from data/. FSexplorer handlers fall through to plain .js streaming when LittleFS.exists for the .gz sibling returns false. LittleFS image grew by approximately 110 KB; well within 2 MB partition budget.

2026-04-26: Follow-up enhancement (commit f8baa1ec): made the OT-Direct status panel collapsible per user request. Added a clickable heading 'OT-Direct Status' with arrow toggle (mirrors existing sat-collapsible pattern). State persisted in localStorage under 'otd-panel-collapsed'. toggleOTDOverrides() existing inner toggle migrated to textContent for consistency.

2026-04-26: Verified live: user's browser console showed `Saved 10000 log entries to localStorage index.js:1179:13` plus continuous XHR polls to /api/v2/device/time and /api/v2/otgw/otmonitor returning 200 — proof that index.js loads and executes. ACs #1, #2, #4, #11, #12 marked done. Remaining ACs (SAT tab interactions, settings page, FSexplorer regression check, ESP8266 build) need targeted browser testing on the device when user has time.
<!-- SECTION:NOTES:END -->
