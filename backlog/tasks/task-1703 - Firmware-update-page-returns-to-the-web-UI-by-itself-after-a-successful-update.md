---
id: TASK-1703
title: Firmware update page returns to the web UI by itself after a successful update
status: In Review
assignee:
  - '@claude'
created_date: '2026-11-07T17:06:00Z'
updated_date: '2026-11-07T22:51:00Z'
labels:
  - webui
  - field-report
dependencies: []
priority: low
ordinal: 816000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
tjfs 2026-11-07 14:21 UTC: after a successful update the user has to press a button to go back to the web UI; it should go there automatically.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a successful firmware or filesystem update the update page waits until the unit answers again and then opens the web UI without a click
- [x] #2 On a failed update the page stays and shows the error
- [ ] #3 Bench evidence (screenshots or capture of the redirect)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Analysis: of the four success paths only the one-action flow (go() in webui/update.html) waited for a click (it showed the Open web UI link). The manual forms (waitForDeviceReboot) and update-success.html already poll /api/v2/health and redirect; the safeboot page (safeboot/src/main.cpp) reloads / after a fixed 20 s and is a separate image, left unchanged (ADR-191, needs its own build).

Change (uncommitted, lane-1703 worktree): go() res.ok branch now says Opening the web UI and calls the existing redirectToHome(3000) to PAGE_BASE?_ota=..., after the beforeunload guard is removed; the Open web UI link stays as open-now. No second poll: U.run only returns ok after doVerify saw health UP, matching fs hash and the new version (one request at a time). Failure branch unchanged: stays, shows error, Try again / Back.

Evidence: new tests/webui/update-auto-return.test.mjs runs the REAL second script of update.html in a vm context (stub DOM, manual timers, fake OTGWUpd.run). OLD (OTGW_UPDATE_HTML=git show lane-1701 copy): tests 4, pass 2, fail 2 (success did not navigate, href stayed /update; prefix variant same). NEW: tests 4, pass 4, fail 0. Mutant adding redirectToHome to the failure branch: pass 3, fail 1 (the page navigated away after a failure). update-orchestrator.test.mjs: 58 tests, 47 pass, 0 fail, 11 skipped. python evaluate.py --quick: 91 checks, 84 passed, 0 failed, 0 warnings.

Not verified: a real browser and a real unit (navigation, beforeunload, timing through safeboot); AC3 bench screenshots need a device and a build. updateServerHtml.h is gitignored and generated at build time, so nothing to commit for it.

Full tests/webui per file (120 s cap each): 38 of 39 files green, 0 fail; v2-settings-sat-followups.test.mjs hangs past 120 s here. It does not read update.html and this lane changes nothing under data/, so it is unrelated to this change (not investigated further).

Lead integration on lane-1707 (on origin/dev 1d96ac6c1 + TASK-1707): patch applied cleanly; update-auto-return 4/4 pass, old page (HEAD copy via OTGW_UPDATE_HTML) 2 fail; update-orchestrator 47 pass 0 fail 11 skipped. Verifier (sound, real code, red/green reproduced): minor findings only; the safeboot page's fixed 20 s reload stays for a follow-up.

Pushed origin/dev 8cbdebc3f (TASK-1703 = 8cbdebc3f, beta.133). Bench .61: the served /update (gzip) contains the new 'Opening the web UI...' path; build green. A real browser update with the redirect was not run (it needs another flash of the shared bench unit): AC3 open, In Review until a user update on beta.133+ confirms it. Follow-up candidate: the safeboot page's fixed 20 s reload (safeboot/src/main.cpp:63).
<!-- SECTION:NOTES:END -->
