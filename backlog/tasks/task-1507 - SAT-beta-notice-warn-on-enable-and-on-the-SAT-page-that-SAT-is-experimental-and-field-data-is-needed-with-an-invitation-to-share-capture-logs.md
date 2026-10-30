---
id: TASK-1507
title: >-
  SAT beta notice: warn on enable and on the SAT page that SAT is experimental
  and field data is needed, with an invitation to share capture logs
status: To Do
assignee: []
created_date: '2026-10-30T06:47:00Z'
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
- [ ] #1 Enabling SAT shows the experimental + field-data step first; cancelling leaves SAT off (browser screenshot before/after)
- [ ] #2 The SAT page shows the beta notice with the capture invitation on a prerelease build and not on a release build (both states shown)
- [ ] #3 SAT default stays off on a fresh settings file (host test or settings dump)
- [ ] #4 evaluate.py design-system gate green; build green
<!-- AC:END -->
