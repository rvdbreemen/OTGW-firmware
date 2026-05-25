---
id: TASK-385
title: 'Fix: text fields render dark in light mode (1.4.2-beta)'
status: To Do
assignee: []
created_date: '2026-04-23 06:53'
updated_date: '2026-05-25 22:28'
labels:
  - bug
  - ui
  - needs-info
dependencies: []
references:
  - 'Discord #beta-testing'
  - user andrebrait
  - '2026-04-23 02:09Z'
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Discord #beta-testing (andrebrait, 2026-04-23 02:09Z): after upgrading from 1.3.10 to latest 1.4.2-beta, text fields render with dark appearance in LIGHT theme. Screenshot attached to Discord message. May be a side-effect of the recent cross-browser color-scheme hardening (commit 7a894f50) or the design-system fonts patch (commit 97b46807). Also possibly mobile-browser specific — need to know browser/OS.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Screenshot from andrebrait saved or inspected to see exact rendering
- [x] #2 Root cause identified (CSS regression in light theme, color-scheme interaction, or mobile-specific)
- [ ] #3 Fix confirmed by andrebrait on 1.4.2-beta hardware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: screenshot + browser/OS info from andrebrait. Possible regressions to check: ds-tokens.css (commit 97b46807), color-scheme: light (commit 7a894f50), or cross-theme input-changed contrast work (ae959676).

2026-04-23: Preventive fix landed on dev (commit c0eb1682). index.css now sets explicit background-color: white; color: black; on the input base rule plus color: black; on .input-normal and .input-changed, closing the asymmetry with dark theme. Root cause: mobile browsers (iOS Safari + some Android Chromium) can honor OS-dark-mode UA text colors for form widgets despite our color-scheme: light declaration, if the CSS does not explicitly set color. Awaiting field validation by andrebrait on 1.4.2-beta hardware.

Triage 2026-05-22: still relevant but no active work in 14+ days; deprioritised. 1.4.2-beta era UI bug; no follow-up screenshot from andrebrait, no recent reports against 1.5.x/1.6.0-beta. May already be fixed by subsequent CSS hardening.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fix committed in c0eb1682 (2026-04-23). Root cause: mobile browsers (iOS Safari, Android Chromium) honour OS dark-mode UA text colours for form widgets despite color-scheme:light, when CSS does not explicitly set color. Fix: explicit background-color:white; color:black on input base rule plus .input-normal / .input-changed in index.css. AC #3 (andrebrait field-confirmation) remains: no further reports on 1.5.x/1.6.0-beta, fix present in current dev.
<!-- SECTION:FINAL_SUMMARY:END -->
