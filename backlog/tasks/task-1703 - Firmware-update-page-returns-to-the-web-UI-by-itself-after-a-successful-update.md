---
id: TASK-1703
title: Firmware update page returns to the web UI by itself after a successful update
status: Done
assignee:
  - '@claude'
created_date: '2026-11-07T17:06:00Z'
updated_date: '2026-11-16T08:25:00Z'
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
- [x] #3 Bench evidence (screenshots or capture of the redirect)
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

AC3 bench by 09, 2026-11-14 23:08, .61 (OTGW32) on 2.0.0-beta.257+47f0ea3, real headless Chrome over CDP (scratchpad tools/otaredirect.mjs): GET /update, the release zip OTGW-firmware-esp32-combo-2.0.0-beta.257+47f0ea3-flash.zip into the one-file field (#oneFiles), Update clicked once, no further input. Timeline: t+2 s steps 1-3 green (same version already running, app step skipped), step 4 'Install the web files' 35 % (272.0 KB / 768.2 KB); t+28 s the page navigated by itself to /?_ota=<ts>; t+31 s the v2 web UI was loaded (nav present, footer v2.0.0-beta.257). No click after Update. Device after: bootcount 651 -> 652 (FS only), lastreset Software reset, hash-check fw 47f0ea3 = fs 47f0ea3, settings API diff [], crashlog unchanged. Screenshots: 09 scratchpad b1703/zip/shot-01-before.png, shot-02-update.png (progress), shot-03/04 (web UI after the redirect), log b1703/zip/redirect.log. Side note: a lone .ino.bin in the one-file field leaves Update disabled (the field asks for the release zip; the separate firmware form takes the .ino.bin), so the first try at 23:03 flashed nothing.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The firmware update page returns to the web UI by itself after a successful update and stays on a failure (fix 8cbdebc3f, beta.133; on origin/dev). AC1: on res.ok the page removes the beforeunload guard, says 'Opening the web UI' and calls redirectToHome(3000) to PAGE_BASE?_ota=...; tests/webui/update-auto-return.test.mjs runs the real update.html script: old page 2 of 4 fail (success did not navigate), fix 4/4. AC2: a failed update keeps the page with its error; a mutant that also redirects on failure fails the test (3 pass, 1 fail). AC3: bench by 09 on .61, beta.257, real headless Chrome, zip one-file update: the page navigated to /?_ota= by itself at t+28 s and the web UI loaded at t+31 s with no click; settings diff empty; screenshots in 09's scratchpad b1703/zip/ (b42924b9b). Out of scope: the safeboot updater's own fixed 20 s reload (safeboot/src/main.cpp:63).
<!-- SECTION:FINAL_SUMMARY:END -->
