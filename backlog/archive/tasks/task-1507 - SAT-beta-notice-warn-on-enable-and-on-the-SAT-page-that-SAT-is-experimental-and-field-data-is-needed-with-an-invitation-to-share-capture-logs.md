---
id: TASK-1507
title: >-
  SAT beta notice: warn on enable and on the SAT page that SAT is experimental
  and field data is needed, with an invitation to share capture logs
status: Done
assignee:
  - '@claude'
created_date: '2026-10-30T06:47:00Z'
updated_date: '2026-10-30T09:57:00Z'
labels:
  - sat webui beta
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 663000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-30: the first beta (TASK-1504) is opt-in: users enable SAT knowingly and, during the beta, see a notice that SAT still needs field data for validation, which also invites them to contribute field datalogs. The v2 UI already opens the SAT onboarding wizard when SAT is enabled (TASK-1012: v2.js:2750, :5000, startSatOnboarding('enable')). Add: (1) a first wizard step that states SAT is experimental in this beta, controls the boiler, and asks for field data, with a link/short how-to for the capture tool; the user confirms before the enable goes through; (2) a persistent, dismissible-per-session notice on the SAT page while the firmware is a beta/alpha build (version.h prerelease word), with the same invitation; (3) SAT stays default off. English UI text, design-system classes (ADR-091), no new API calls beyond the existing ones (2-concurrent rule).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Enabling SAT shows the experimental + field-data step first; cancelling leaves SAT off (browser screenshot before/after)
- [x] #2 The SAT page shows the beta notice with the capture invitation on a prerelease build and not on a release build (both states shown)
- [x] #3 SAT default stays off on a fresh settings file (host test or settings dump)
- [x] #4 evaluate.py design-system gate green; build green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 (coordinator): tests/webui/sat-beta-notice.test.mjs 10 FAIL on the old UI, 20 PASS on the new; all 28 webui tests pass; node --check OK; evaluate 79/0/0; screenshots (mock device, before/after) in wt-1507/screenshots-1507/. Built alpha.550+9fdfb6f, OTA app+fs to bench .63 (Classic + S3 Mini): 174 settings unchanged; v2.html/v2.js on the device contain the notice and the beta step; real-device headless render (CDP, click on the SAT nav button) shows #satBetaNotice display:block with the full text (C:\Users\rvdbr\AppData\Local\Temp\claude\D--Users-Robert-Documents-GitHub-RvdB-OTGW-firmware\00186862-ab4a-4f89-8e31-0f2409fa6a11\scratchpad\device-alpha550-sat.png). SAT default off: SATtypes.h:470 bool bEnabled = false (source check). Pushed 9fdfb6f65, zip posted msg 1565665742509178962.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Enabling SAT opens the SAT wizard on a 'Before you enable SAT' step (experimental, controls the boiler, field data wanted, capture tool + where to share); Continue needs a ticked box, Cancel leaves SAT off and POSTs nothing. The SAT page shows the notice on prerelease firmware, dismissible per browser session (sessionStorage), reusing the device/info snapshot (no new request). Evidence: webui test red 10 / green 20, all webui tests pass, evaluate green, real-device render on alpha.550. Not verified: the Settings-save enable path through the same wizard (shares startSatOnboarding('enable')), the wizard step on the real device (covered by the mock-device test), a settings dump on a fresh file (source check only).
<!-- SECTION:FINAL_SUMMARY:END -->
