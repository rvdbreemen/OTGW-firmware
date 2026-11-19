---
id: TASK-2918
title: >-
  Web UI: the one-time update-check question (TASK-2023) opens as a modal 30 s
  after page load on any page and covers what the user is looking at, until
  someone on that device answers it
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T14:13:00Z'
updated_date: '2026-11-19T16:58:00Z'
labels:
  - ui
  - webui
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1056000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by 09 on .63 (beta.392 bench of TASK-2914): the question covered part of the connection map. Code (data/v2.js maybeUpdateCheck/updAsk): the asked state is the DEVICE setting updatecheckasked, not per browser; with UI_UPDATE_CHECK_DEFAULT_MODE 1 the question opens as a sim-modal with backdrop about 30 s after every page load (setTimeout(maybeUpdateCheck, 30000)), on whatever page is open, in every browser, until Yes or No is chosen on that device; a click on the backdrop means not now and asks again on the next visit. A fresh browser profile is not the trigger; a device where nobody answered is (every fresh install, every settings reset, bench units). Options: A) keep the modal but only open it on the Home page start and never while the user is on another page (small change, still a modal); B) a non-modal header notice next to the version chip with Yes/No (never covers content; more UI work, i18n keys exist); C) leave as is and let bench prep set updatecheckasked=true. Recommendation to weigh: B. No build before the 20:00 beta (coordinator).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Reproduced in a host test: the question opens over another page after 30 s on a device with updatecheckasked=false; behaviour after the fix shown red/green
- [x] #2 Chosen option implemented; the question never covers another dialog or page content the user is working in; Yes/No still write updatecheck then updatecheckasked (ADR-262 unchanged)
- [x] #3 update-check-github.test.mjs and test_update_check_settings.py green; evaluate --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Coordinator 2026-11-19: tonight option C (09 sets updatecheckasked=true in bench prep, no build before the 20:00 beta); B for the next beta if ADR-262 leaves the form open. Checked ADR-262: Robert's decision (line 49 'off, with a one-time question'), Decision (96), Settings (114-119) and the Decision Contract fix only a one-time question that waits for the first-time wizard and never covers another dialog; no modal or position. Only Consequences/Negative (189) says 'one more dialog at first use per device', a described effect, not a decision. So B is within ADR-262 (no question to Robert); with B that Consequences sentence is no longer literal: the Accepted ADR stays unedited, an amendment ADR only if the coordinator wants one.

Implemented option B (data/v2.js updAsk): the question is a header chip 'Check for new versions?' placed right after the identity group (.hdr-net), outside it because v2.css hides .hdr-net at <= 480 px; the explanation (names api.github.com) and No/Yes sit in a popover that opens only on a click; a second click or Escape closes it (not now, the chip stays). Yes/No unchanged: updatecheck then updatecheckasked, one POST at a time. The chip gets z-index 61 + opacity 1 (.hdr-id opacity .95 made a stacking context that put the next chips over the popover) and the open popover is shifted inside the viewport. Same placement fix for the 'New version available' notice (hdrUpd): it sat inside .hdr-net too, so phones never saw it (TASK-2023 gap). Evidence: tests/webui/update-check-github.test.mjs 14/14 on the fix; on the old v2.js 11 pass 3 fail ('no modal backdrop over the page', 'the question is shown'). Screenshots (synthetic device): scratchpad upd2918c-1400-closed/open.png and upd2918c-390-closed/open.png: closed covers nothing; open at 390 px rect 24..382 (inside the viewport), above the status pill. test_update_check_settings OK, header-controls-same-height 2/2, i18n-no-hardcoded 3/3, tab-strip-no-vscroll 1/1, evaluate --quick 88/0.

Separate finding (for the next beta's release notes, coordinator 2026-11-19): since TASK-2023 (beta.387) the 'New version available' notice was never visible on a phone or any window <= 480 px wide: it sat inside the .hdr-net identity group, which v2.css hides at that width (@media(max-width:480px){.hdr-net{display:none}}). Fixed in the same commit by placing the notice (and the new chip) right after that group. Plan (coordinator): not in the 20:00 beta of 2026-11-19; for the 08:00 beta: rebase after the 20:00 release, re-bump, build in the coordinator's slot, bench .63 by 09 (updatecheckasked=false, chip after 30 s, no modal, popover inside the viewport at 390 px, No writes both settings).

2026-11-19 bench .63 on beta.393 (fb753d0, never pushed), 09: points 1,2,3,5,6 PASS, point 4 FAIL: at 390x844 mobile emulation the open popover ran L=175 R=533, 143 px past the right edge. Cause (09's measurement, reproduced here): the overflowing popover widens the visual viewport (innerWidth 390 -> 533) and show() clamped against innerWidth. My earlier 390 px screenshot ran with mobile:false, where innerWidth stays 390, so it missed this. Fix: show() places the popover from the chip's left and the popover width against document.documentElement.clientWidth (8 px margin). New test tests/webui/update-ask-chip-phone.test.mjs (real v2.html/v2.js, mobile:true 390x844): red on fb753d0 with {l:175,r:533,iw:533,sw:533}, green on the fix. Rebased onto 5e's 4bb2097ed (393) as beta.394.

Correction to the previous note: the 394 build on 5e's 4bb2097ed (5f5e3db, bench PASS) was not pushed; the coordinator chose to restack because 4bb2097 panicked on .61 under the heap hold. Landed as beta.393 ffe5dffeb directly on 3fb853b04, pushed 2026-11-19. Bench 09 on .63 (ffe5dff, OTA app+fs 17:51): 390x844 mobile popover L=24 R=382, innerWidth 390; desktop popover L=561 R=941 under the chip, no horizontal scroll; chip after ~30 s, no backdrop; No -> updatecheck false/asked true, reload no chip; Yes -> true/true; 0 JS exceptions; 5-min smoke bootcount flash+2, reboot_log only the OTA line, MQTT up, freeheap 34440. Zip posted to Discord (message 1573019487765143743).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The one-time update-check question (ADR-262) is a header chip after the identity group instead of a modal over the page; the explanation and No/Yes open in a popover on a click, which stays on screen on phones (clamped to documentElement.clientWidth). The New version available notice is visible again at <= 480 px. AC1: update-check-github.test.mjs new case red on the old v2.js (modal backdrop, no chip), green on the fix; update-ask-chip-phone.test.mjs red on fb753d0 {l:175,r:533,iw:533}, green on the fix. AC2: implemented in beta.393 ffe5dffeb; Yes/No write updatecheck then updatecheckasked; bench 09 on .63 PASS (popover 24..382 at 390 px, no backdrop, 0 JS errors). AC3: update-check-github 14/14, test_update_check_settings OK, evaluate --quick 0 FAIL.
<!-- SECTION:FINAL_SUMMARY:END -->
