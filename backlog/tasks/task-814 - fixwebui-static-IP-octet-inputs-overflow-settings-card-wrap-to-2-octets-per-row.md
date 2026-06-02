---
id: TASK-814
title: >-
  fix(webui): static-IP octet inputs overflow settings card, wrap to 2 octets
  per row
status: In Progress
assignee:
  - '@claude'
created_date: '2026-06-02 17:14'
updated_date: '2026-06-02 21:28'
labels: []
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
On the 2.0.0 settings page the WiFi/Ethernet fixed-IP rows render four segmented octet inputs, but only two show per row and the rest are clipped. Root cause: the fixed-ip-section is a .settingDiv direct child of .settings-group-body, so the broad fill rule '.settings-group-body > .settingDiv input[type=text] { flex:1; min-width:80px }' (components.css:791) matches the nested .octet-input fields and overrides their compact 38px width. The stretched octets overflow the ~520px card and wrap; .fixed-ip-section overflow:hidden clips the second row, and the squeezed col-1 also wraps the field labels. Desired: all four octets in one compact box that fits the row (see maintainer screenshot).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 All four octet inputs render on one line inside a compact octet-group box that fits within the settings card
- [x] #2 Field labels (IP Address, Subnet Mask, Gateway, DNS) no longer wrap to two lines
- [x] #3 Fix scoped so it does not regress the normal full-width text/password/number inputs in other settings rows
- [x] #4 Build green (python build.py) and evaluate.py --quick shows no new failures
- [x] #5 Prerelease tag bumped per versioning policy
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Follow-up (alpha.144): first fix made the four octets fit one row, but 3-digit values (192, 255) clipped to two chars. Generic '.settingDiv input[type=text] { font-size:12pt }' (specificity 0,2,1) outweighs '.octet-input { font-size: var(--fs-sm) }' (0,1,0), so octets rendered at 12pt and overflowed the 38px box. Re-asserted font-size:var(--fs-sm) in the high-specificity reset and bumped width 38->42px.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added a higher-specificity CSS reset in components.css so the broad '.settings-group-body > .settingDiv input[type=text]' fill rule no longer stretches the nested static-IP octet inputs: '.settings-group-body > .settingDiv .octet-group .octet-input { flex:0 0 auto; min-width:0; width:38px }', plus '.fixed-ip-fields .fixed-ip-row > .fixed-ip-field-label { flex:1 1 auto; min-width:0 }' to give the label the row slack so the compact octet box right-aligns. All four octets now fit one box; labels stop wrapping. No new DS classes (ADR-091 drift gate green). Shipped as alpha.143; esp32+esp8266 build clean, evaluator 0-fail. Visual confirmation on device welcome.
<!-- SECTION:FINAL_SUMMARY:END -->
