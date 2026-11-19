---
id: TASK-2918
title: >-
  Web UI: the one-time update-check question (TASK-2023) opens as a modal 30 s
  after page load on any page and covers what the user is looking at, until
  someone on that device answers it
status: To Do
assignee: []
created_date: '2026-11-19T14:13:00Z'
updated_date: '2026-11-19T14:20:00Z'
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
- [ ] #1 Reproduced in a host test: the question opens over another page after 30 s on a device with updatecheckasked=false; behaviour after the fix shown red/green
- [ ] #2 Chosen option implemented; the question never covers another dialog or page content the user is working in; Yes/No still write updatecheck then updatecheckasked (ADR-262 unchanged)
- [ ] #3 update-check-github.test.mjs and test_update_check_settings.py green; evaluate --quick no new FAIL
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Coordinator 2026-11-19: tonight option C (09 sets updatecheckasked=true in bench prep, no build before the 20:00 beta); B for the next beta if ADR-262 leaves the form open. Checked ADR-262: Robert's decision (line 49 'off, with a one-time question'), Decision (96), Settings (114-119) and the Decision Contract fix only a one-time question that waits for the first-time wizard and never covers another dialog; no modal or position. Only Consequences/Negative (189) says 'one more dialog at first use per device', a described effect, not a decision. So B is within ADR-262 (no question to Robert); with B that Consequences sentence is no longer literal: the Accepted ADR stays unedited, an amendment ADR only if the coordinator wants one.
<!-- SECTION:NOTES:END -->
