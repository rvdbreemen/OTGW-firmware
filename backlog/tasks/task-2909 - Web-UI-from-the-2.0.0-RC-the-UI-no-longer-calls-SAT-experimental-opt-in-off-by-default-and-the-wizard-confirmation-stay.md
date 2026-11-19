---
id: TASK-2909
title: >-
  Web UI: from the 2.0.0 RC the UI no longer calls SAT experimental (opt-in, off
  by default and the wizard confirmation stay)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-19T09:00:00Z'
updated_date: '2026-11-19T12:30:00Z'
labels:
  - webui
  - sat
  - release
milestone: 2.0.0
dependencies: []
ordinal: 1052000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, DM 17:48-17:51 2026-11-18 (1572669805515903007, 1572669870414499931, 1572670004963451012, 1572670569961496768), relayed by the coordinator: 'Als we 2.0.0 releasen dan gaan we het woord experimenteel eraf halen. SAT krijgt stabiel niveau'; opt-in, off by default and the wizard confirmation stay. Web UI side of TASK-2034 (otgw-docs, docs side, AC4 reported these strings to ba). Strings that call SAT itself experimental: v2.js onb_beta_ack (wizard confirmation), sat_beta_head and its fallback SAT_BETA_HEAD (SAT beta notice), and their nl/de/el pack entries. NOT in scope: the on/off heat-pump 3 h type labels (satsource help, pwm_help_hp, enum_onoff_hp) stay experimental, Robert 'B voor warmtepomp' (DM 1572672969744851067). Key names stay. Lands with the 2.0.0 RC commit set when the coordinator calls it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 No web UI string (v2.js, v2.html, i18n nl/de/el) calls SAT itself experimental; the heat-pump 3 h type labels still do; a test fails on the old texts and passes on the new
- [x] #2 The wizard confirmation still asks the user to confirm that SAT controls the boiler, and the beta notice still shows on prerelease builds; key names unchanged
- [x] #3 Wording agreed with otgw-docs (TASK-2034) so UI and manual say the same; web UI tests green
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Wording agreed with otgw-docs (TASK-2034, ch05:12, h05:8, ch01:59/h01:68 'SAT is stable from 2.0.0; it stays opt-in and is off by default'). Change onb_beta_ack ('takes over control of my boiler'), sat_beta_head + SAT_BETA_HEAD ('SAT controls your boiler. This is a beta firmware.'), and the opening sentence of sat_beta_body ('Field data helps us improve it.' instead of 'to validate it') in en/nl/de/el; key names stay; the heat-pump 3 h type keeps 'experimental' (Robert B). Guard test tests/test_sat_not_experimental.py; update the TASK-1507 assertions in tests/webui/sat-beta-notice.test.mjs (they required 'experimental'). Channel in the body stays #alpha-testing (where captures go today, TASK-1507); a move to #beta-testing is a separate decision, raised to the coordinator.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-19 implemented (lane/ba-2023, uncommitted until RC timing): wording agreed with otgw-docs (95). tests/test_sat_not_experimental.py 5/5 green, red 3/5 on the old texts (the 2 guard checks, boiler named and heat-pump type still experimental, green on both). tests/webui/sat-beta-notice.test.mjs updated (TASK-1507 asserted 'experimental'; now 'beta firmware' and not 'experimental'): 1/1 green, red on the origin v2.js. Static web UI: sat-wizard-tuning-text 1/1, i18n-lang 6/6, i18n-followups 4/4, i18n-no-hardcoded 3/3, v2-undefined-names 2/2. evaluate --quick 88/0.

2026-11-19 coordinator decided B: the beta notice points to #beta-testing (shows on every prerelease build, public betas included; matches the stable docs, Robert's decision A of 2026-11-18). sat_beta_body en/nl/de/el + SAT_BETA_BODY; sat-beta-notice.test.mjs and test_sat_not_experimental.py assert it.

beta.390 2f1b9f48a (on d3 389 931d1cdef): tests 5/5 + sat-beta-notice ALL PASS + i18n-followups 4/4, evaluate 88/0, build EXIT=0. 09 bench .63: UI points 1-4 PASS (notice en/nl with #beta-testing, 0x experiment* on SAT page, wizard en/nl new ack, Cancel closes, heat-pump type keeps (experimental), 0 JS exceptions, 0 writes); 16 min regression running. Push waits for d3 389 push + coordinator go.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Pushed as beta.390 (2f1b9f48a, FF on d3's 389 931d1cdef; zip posted, Discord message 1572951893548728445). AC1: tests/test_sat_not_experimental.py red 3/5 on the old texts, 5/5 on the fix (heat-pump type still experimental in en/nl/de/el); 09 bench .63: 0x experiment* on the SAT page, heat-pump type option keeps (experimental)/(experimenteel). AC2: key names unchanged; 09 bench en/nl: beta notice shows on 390, wizard ack names the boiler, Cancel writes nothing, 0 JS exceptions; sat-beta-notice.test.mjs ALL PASS. AC3: wording agreed with otgw-docs (TASK-2034 on origin 01d69164e, #beta-testing on both sides); i18n-followups 4/4, handback/update-check/progmem OK, evaluate --quick 88/0, build EXIT=0.
<!-- SECTION:FINAL_SUMMARY:END -->
