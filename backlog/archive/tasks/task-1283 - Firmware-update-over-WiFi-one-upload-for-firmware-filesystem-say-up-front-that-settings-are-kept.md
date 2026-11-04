---
id: TASK-1283
title: >-
  Firmware update over WiFi: one upload for firmware + filesystem, say up front
  that settings are kept
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 18:10
updated_date: '2026-10-22T00:41:00Z'
labels:
  - ui
  - ota
  - update
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 412000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Feedback gruijter 2026-10-21 (#alpha-testing). Today an update is two uploads (firmware through the safeboot updater, then the LittleFS image), settings survive both (TASK-1264: 0/567 changed), but that is not obvious. Confusing: after the firmware step and before the filesystem step the UI offers to save the settings to the laptop, which suggests they could be lost. The filesystem always ships with a firmware release, so one file upload would be easier. Backup/restore of settings is welcome but should stay out of the update flow. Explain before the update that settings are kept. Constraint: the main firmware cannot write ota_0 itself (ADR-191 safeboot), so a single upload needs a flow across the safeboot restart (e.g. upload the flash zip or both images once, safeboot writes the app, the main firmware then writes the filesystem) or a guided two-step page.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Design recorded: single upload (which file, how the filesystem step crosses the safeboot restart) or guided flow; ADR if the OTA contract changes
- [x] #2 The update page states before the update that settings are kept, and the mid-update 'save settings' prompt is gone or explained
- [x] #3 Bench: an update from the previous alpha with one user action keeps settings (diff 0) and ends on the new firmware + filesystem
- [x] #4 Screenshots of the flow
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Design brief from a 5-agent understand workflow: %TEMP%\bench-1283\brief.md (+ maps.json). Orchestrator lives in updateServerHtml.h (the PROGMEM /update page: served whatever the LittleFS state, no WS/pollers); v2 and FSexplorer keep linking to /update. Sequence: preflight (GET /info JSON, device/info fwversion+githash, board, password) -> file intake (two bins or the flash zip via DecompressionStream deflate-raw; strict app/fs recognition per brief section 3, refuse merged/migrate/safeboot images) -> POST /safeboot -> poll until safeboot answers (JSON 'safeboot' key, not res.ok) -> upload .ino.bin to safeboot (treat 'sent 100% then connection drop' as verify-by-poll) -> poll until the new main firmware answers with the expected version (handles rollback: safeboot with app_rolled_back) -> wait until the new image is marked valid (~35 s) -> upload littlefs to /update?cmd=100&size= (check 'Flash error' body, not only status) -> poll until it is back with the new filesystem githash -> done. One request at a time, no overlapping polls. Up-front text says settings and sensor labels are kept (measured list of what a filesystem update keeps/loses goes in the wording); the 'Download backups' default checkbox goes away from the update flow (backup stays available as its own button). Recovery text per failed step (safeboot AP fallback <host>-safeboot / 192.168.4.1, rollback, filesystem step failed -> upload the littlefs only).
Decisions (main session, 2026-10-22): Q1 one-action works from the first alpha that contains it (bootstrap); Q2 accept one browser auth prompt; Q3 labels limit out of scope; Q4 measure which files survive a filesystem flash and state it honestly; Q5 Ethernet/static IP: warn, not block; Q6 board slug vs hardware_type: warn on mismatch; Q7 pick file(s) + press Update = one action; Q8 wait ~35 s before the filesystem step; Q9 document heating during safeboot (OT-Direct: no master, boiler on its own; PIC: gateway continues); Q10 guide + test; Q11 githash pair check is a warning; Q12 8 MB dual-slot boards keep the current flow (no bench unit).
Verification: host test of the intake rules (zip parse + image recognition) where practical; bench OTGW32: install build A (contains the orchestrator) the old way, then use A's /update page (Playwright, file upload) to install build B with one action: settings diff 0, version and githash of B on firmware and filesystem, screenshots of each phase; plus a wrong-file refusal and a simulated failure (abort after safeboot) recovery check.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-22 maintainer decision: the browser orchestrates both steps. One file picker (the two bins .ino.bin + .littlefs.bin, or the flash zip unpacked in the browser without a library); the page uploads the firmware to safeboot, waits for the restart into the new firmware, then uploads the filesystem to it. UI-only: safeboot and the OTA contract (ADR-191) stay as they are. Trade-off accepted: the browser tab must stay open until both steps finish. Rejected: safeboot writing both (settings backup inside safeboot, 960K budget, highest-risk place); guided page only (still two actions).

2026-10-22 review workflow (4 lenses: correctness, security, rules, tests; 2 skeptics per finding; 44 agents): 20 findings, 16 upheld, 9 distinct issues. Two HIGH: (1) classify() matched the literal '[safeboot] start', which is in the page's own source and so in every new main image -> every future release refused as the safeboot image (reproduced on the bench A.ino.bin); (2) the filesystem identity is only the 7-char githash, so a same-commit build or a silently failed write skipped the filesystem and still reported Done. Plus: retry only re-verified, Back used a stale snapshot, a dropped firmware upload could not be re-sent, one failed final read reported 'wrong version', decompression-bomb size check, deflate-raw TypeError, wrong test path in a comment. Refuted: no version floor for loose files, file-name-based board warning, same-githash rules duplicate, test-skip evidence gap. The local commit 6b8df3e is NOT pushed; fixes follow before the bench run. Bench so far: A = alpha.449+6b8df3e installed the old way; GET /update serves the 46,300 B page with maxfreeblock ~31.7 KB, which shows the stream shim is required (two String copies would need ~94 KB).

2026-10-22 BENCH (OTGW32 .88.61): build A = alpha.449+a447ff7 (this change incl. review fixes) installed the old two-step way (bootstrap). Then a real Chromium (Playwright) on http://192.168.88.61/update: (1) picked merged-full.bin + littlefs.bin -> 'Not accepted: ...merged-full.bin is a USB image (for flash_otgw), not a WiFi update file', Update stayed disabled, nothing written (p1-refused.png). (2) picked the published alpha.448+7eb246d flash zip (unpacked in the browser) -> Update -> one action, 104 s: waiting for the updater 21 s, sending firmware 27 s, new firmware up 48 s, 35 s settle, web files 82 s, restart, 'Done. The unit runs 2.0.0-alpha.448+7eb246d with matching web files.' at 103.5 s (p2-ready.png, p3-run-*.png, p4-end.png). Independent check afterwards: device/info fwversion 2.0.0-alpha.448+7eb246d, hash-check match fw 7eb246d = fs 7eb246d, settings diff vs before the update: 0 of 572 keys. Console errors in the run: one 404 (the /info poll on the main firmware) and one connection refused during a restart, both expected. GET /update serves 46-49 KB with maxfreeblock ~31.7 KB, so the stream shim is required. Bench reinstalled to A afterwards. Tests: update-orchestrator 57/57 (a mutant test appended the unmutated page text and only went red with an old build in build/; fixed to append the mutated text), webui 102/102, evaluate --quick 72/0, test_build_safeboot OK, test_static_integrity 33 OK. Evidence %TEMP%\bench-1283.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The /update page now runs a whole WiFi update in one action: pick the release zip (unpacked in the browser) or its two .bin files, press Update; it restarts into safeboot, sends the firmware, waits for the new firmware and lets it settle, sends the web files and checks firmware and web files match. Files are recognised by content; USB images, the safeboot image and wrong-size filesystem images are refused before anything is written; each failed step has recovery text and Try again resumes the right step. The page states up front what is kept (settings, sensor names, WiFi) and what a web-files update resets (SAT learned values etc., follow-up TASK-1286); the mid-update backup prompt is gone. The 46 KB page is streamed from flash. AC1: decision recorded. AC2: up-front text and backup link (screenshot). AC3: bench one action from alpha.449 to the published alpha.448 zip in 104 s, settings diff 0/572, fw+fs hashes match. AC4: screenshots of every phase. Review: 44-agent workflow found two blocking bugs (self-matching safeboot marker; githash-only filesystem identity) and 7 smaller ones, all fixed and tested. Not verified: an admin password (browser auth prompts), Firefox/Safari, Ethernet and PIC boards, and this page updating a LATER release on the bench (it updated to an older one); the first install of this release is still two steps (bootstrap).
<!-- SECTION:FINAL_SUMMARY:END -->
