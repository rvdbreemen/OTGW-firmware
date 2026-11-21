---
id: TASK-2998
title: >-
  Web UI: wizard primary buttons and the injected-values chip fail contrast
  after TASK-2956 (inline color #04222c on --accent, chip fill --zone-warn)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T08:01:00Z'
updated_date: '2026-11-21T08:46:00Z'
labels:
  - webui
  - a11y
  - regression
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1123000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Regression from TASK-2956, found by otgw-ux-designer (impeccable review, 2026-11-21). TASK-2956 made the light --accent #016f83 so white text passes; six inline styles in v2.js still hard-code color:#04222c on background:var(--accent) (onboarding and SAT setup wizard primary buttons and the Done check marks, v2.js:8140, 8241, 8311, 8421, 8486, 8495 at origin/dev 6804af755): 2.84:1 in light. The injected-values chip on the connection map (v2.html:727) draws its text with fill var(--zone-warn) on a 16 % warn tint: 1.91:1. The contrast test of TASK-2956 only probed CSS rules, not inline styles.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The six inline styles use color:var(--on-accent); in light and dark the text on --accent measures at least 4.5:1
- [x] #2 The chip text uses var(--status-warn-text) and measures at least 4.5:1 on its tint in light and dark
- [x] #3 A gate test fails on any inline style in v2.js or v2.html that sets background:var(--accent) together with a colour other than var(--on-accent): red on the code before this task, green after
- [x] #4 esp32-combo build and a NAS smoke by otgw-bench-tester: onboarding and SAT setup wizard buttons and the chip, light and dark
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 otgw-webui: the six inline styles (v2.js:8140, 8241, 8311, 8421, 8486, 8495) now use color:var(--on-accent); #cnOvrTxt (v2.html:727) fill var(--status-warn-text). New gate tests/webui/inline-accent-contrast.test.mjs: static gate (inline background:var(--accent) with a colour other than var(--on-accent) fails) plus a browser measurement of every such pair and the chip, light and dark. RED on origin/dev 6804af755: 1 pass 2 fail, light 2.84 for all six and 1.91 for the chip (dark 7.78 / 5.16 passed). GREEN on the fix 3/3: light 5.83 / 6.02, dark 7.9 / 5.4. Also green: contrast-tokens 3/0, sat-wizard-tuning-text, sat-area-sensor-mapping, home-appliance-label, i18n-no-hardcoded 3/0, sat-beta-notice, mission-control-follows-theme 2/0. evaluate --quick: 0 failed. Evidence OTGW-build-artifacts-safe/evidence-TASK-2998/ (red-old.txt, green.txt). Not checked by eye: the wizard screens (asked for in the NAS smoke).

2026-11-21 build and smoke: esp32-combo beta.409+ea0f5ea SUCCESS (sha256 c7608828354f8bf9b792075dbea1e07a617705c30b8bb8fc5affce964ab025a2, OTGW-build-artifacts-safe/beta.409+ea0f5ea/). otgw-bench-tester NAS smoke on .63 PASS: fwversion/version.hash ea0f5ea; onboarding primary button and SAT setup [data-sa=begin] light 5.83 (white on rgb(1,111,131)), dark 7.9; round Done check measured as a probe with the same inline style 5.83 / 7.9 (the real Done screen needs a saving wizard run, not done); #cnOvrTxt light 6.8 / dark 7.51 against the panel (the host test measures against the chip rect: 6.02 / 5.4). Evidence OTGW-field-data/bench-63/2026-11-21_task2998-contrast/t2998-ui-63/.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Six inline styles in v2.js (onboarding and SAT setup wizard primary buttons, Done check marks) use color:var(--on-accent) instead of #04222c; the injected-values chip text uses var(--status-warn-text). AC1/AC2: host measurement light 5.83 / 6.02, dark 7.9 / 5.4 (was 2.84 / 1.91 in light); NAS smoke on .63 confirms 5.83 / 7.9 for both wizards. AC3: tests/webui/inline-accent-contrast.test.mjs, RED on origin/dev 6804af755 (1 pass 2 fail), GREEN 3/3. AC4: build beta.409+ea0f5ea, otgw-bench-tester smoke PASS with screenshots per wizard per theme. Not verified: the real Done screen (probe with the identical inline style instead).
<!-- SECTION:FINAL_SUMMARY:END -->
