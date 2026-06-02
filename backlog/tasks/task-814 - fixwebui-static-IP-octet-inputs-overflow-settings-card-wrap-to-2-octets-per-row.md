---
id: TASK-814
title: >-
  fix(webui): static-IP octet inputs overflow settings card, wrap to 2 octets
  per row
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 17:14'
updated_date: '2026-06-02 17:14'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the 2.0.0 settings page the WiFi/Ethernet fixed-IP rows render four segmented octet inputs, but only two show per row and the rest are clipped. Root cause: the fixed-ip-section is a .settingDiv direct child of .settings-group-body, so the broad fill rule '.settings-group-body > .settingDiv input[type=text] { flex:1; min-width:80px }' (components.css:791) matches the nested .octet-input fields and overrides their compact 38px width. The stretched octets overflow the ~520px card and wrap; .fixed-ip-section overflow:hidden clips the second row, and the squeezed col-1 also wraps the field labels. Desired: all four octets in one compact box that fits the row (see maintainer screenshot).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 All four octet inputs render on one line inside a compact octet-group box that fits within the settings card
- [ ] #2 Field labels (IP Address, Subnet Mask, Gateway, DNS) no longer wrap to two lines
- [ ] #3 Fix scoped so it does not regress the normal full-width text/password/number inputs in other settings rows
- [ ] #4 Build green (python build.py) and evaluate.py --quick shows no new failures
- [ ] #5 Prerelease tag bumped per versioning policy
<!-- AC:END -->
