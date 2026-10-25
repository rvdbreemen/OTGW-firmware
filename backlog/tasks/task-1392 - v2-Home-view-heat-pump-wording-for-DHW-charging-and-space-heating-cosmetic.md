---
id: TASK-1392
title: 'v2 Home view: heat-pump wording for DHW charging and space heating (cosmetic)'
status: To Do
assignee: []
created_date: '2026-10-25T14:34:00Z'
updated_date: '2026-10-25T15:10:00Z'
labels:
  - needs-maintainer-ok
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 531000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
maarten_41762 2026-10-25 (#alpha-testing 1563922208718463147, heat pump, screenshot): for a heat pump the Home view's DHW state reads 'Hot water running · tap open' (v2.js:1048) and the flow is labelled as going to radiators, while on a heat pump a 3-way valve sends the flow to a DHW storage tank to charge it, and space heating is often floor heating. Suggested: DHW state 'Domestic hot water demand active, charging buffer' when the heating source is a heat pump (or MsgID 3 HB bit 3 storage tank), and a neutral 'space heating' label instead of 'radiators'. Low priority (tester: purely cosmetic). Pure UI text, no behaviour change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With heating source heat pump (or a storage tank) the DHW state and the flow label use the tank-charging wording; gas boilers keep today's text (screenshot both)
- [ ] #2 'Radiators' label replaced by a neutral 'space heating' where it names the CH circuit
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-25 tester feedback: needs Robert's OK before implementation (testers have no admin control).
<!-- SECTION:NOTES:END -->
