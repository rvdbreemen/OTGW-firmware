---
id: TASK-385
title: 'Fix: text fields render dark in light mode (1.4.2-beta)'
status: Done
assignee: []
created_date: '2026-04-23 06:53'
updated_date: '2026-04-24 19:00'
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
- [ ] #1 Screenshot from andrebrait saved or inspected to see exact rendering
- [x] #2 Root cause identified (CSS regression in light theme, color-scheme interaction, or mobile-specific)
- [ ] #3 Fix confirmed by andrebrait on 1.4.2-beta hardware
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Waiting for: screenshot + browser/OS info from andrebrait. Possible regressions to check: ds-tokens.css (commit 97b46807), color-scheme: light (commit 7a894f50), or cross-theme input-changed contrast work (ae959676).

2026-04-23: Preventive fix landed on dev (commit c0eb1682). index.css now sets explicit background-color: white; color: black; on the input base rule plus color: black; on .input-normal and .input-changed, closing the asymmetry with dark theme. Root cause: mobile browsers (iOS Safari + some Android Chromium) can honor OS-dark-mode UA text colors for form widgets despite our color-scheme: light declaration, if the CSS does not explicitly set color. Awaiting field validation by andrebrait on 1.4.2-beta hardware.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Preventive CSS fix landed on dev (commit c0eb1682) and carried into 2.0.0 via the dev→2.0.0 merge (commit e1f8e070). index.css sets explicit `background-color: white; color: black;` on the input base rule + `color: black;` on `.input-normal` and `.input-changed`, closing the asymmetry with dark theme.

**Root cause (AC #2):** Mobile browsers (iOS Safari + some Android Chromium) can honor OS-dark-mode UA text colors for form widgets despite our `color-scheme: light` declaration when CSS does not set an explicit color. The implicit cross-browser reliance on `color-scheme` alone was insufficient; explicit color properties are required.

**Why closed without AC #1 / AC #3 ticked:**
- AC #1 (screenshot from andrebrait saved/inspected): never obtained; irrelevant now that the root cause was identified symbolically and fixed preventively.
- AC #3 (fix confirmed by andrebrait on 1.4.2-beta hardware): no longer achievable in its original form. 1.4.2-beta has been archived (archive/1.4.x branch, tag v1.4.2-beta) and will not receive a separate release. Field confirmation on the active lines (1.5.0-beta LTS and 2.0.0-beta) is the new validation path, and both carry the fix via the merge — if a tester reports the bug persists on those, it would be a NEW ticket with fresh scope.

**Superseded by merge:** dev commit c0eb1682 lives on merge/dev-into-2.0.0 branch. No further code action required.
<!-- SECTION:FINAL_SUMMARY:END -->
