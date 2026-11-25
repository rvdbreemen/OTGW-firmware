---
id: TASK-3012
title: >-
  Web UI: SAT page and Home polish after TASK-2957/2959 (ux checks of beta.413):
  repeated pause message, chart overlay over the axis, MM=T in the history while
  paused, P3 copy and a11y, a flaky test
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-21T15:44:00Z'
updated_date: '2026-11-25T05:38:00Z'
labels:
  - webui
  - ux
  - a11y
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1130000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up of TASK-2957 and TASK-2959, from the otgw-ux-designer critique (lane/ba-2959) and the visual check of the beta.413+da42aca screenshots on .61 (OTGW-field-data/bench-otgw32/2026-11-21_ui-da42aca/). P2 (visual): (1) in PAUSED_OT / PAUSED_OUTSIDE the old hint lines h_mon.sat_ot_lost / sat_outside_missing repeat the headline and sub-line in the messages panel (three times the same news); (2) the control-loop chart note 'No new control data: SAT is paused.' sits over the top y-axis label of the first panel; (3) while active is false the history still records current_modulation (100 % at the MM=T hand-back) and PID, so the chart shows a full modulation band while Control status shows a dash. P3: 'Idle · Anti-cycling' / 'Post-cycle' are firmware labels (spec T2 3.1 says 'anti-cycling pause'); the messages badge aria-label 'Messages: N' does not contain the visible text 'N messages' (WCAG 2.5.3); takeover + STOPPED sub-line asks to switch SAT off and on while the switch reads 'Off · takeover active'; no hysteresis between WAITING and IDLE; STOPPED (needs a person) looks the same amber as PAUSED (fixes itself). Test: home-heatpump-wording waits a fixed sleep(800) after the sat/status request instead of waiting for the heat-pump re-render (flaky once on the NAS). Environment note: the NAS Chromium has no emoji font (preset icons render as boxes in 09's screenshots).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 In PAUSED_OT and PAUSED_OUTSIDE the matching hint line is not shown in the messages panel (the headline and sub-line carry it); it still shows when the state does not cover it
- [x] #2 The control-loop chart note does not overlap the axis labels in any state (screenshot light and dark)
- [x] #3 While active is false no modulation or PID value is added to the SAT history (drawn as a gap); a browser test feeds an inactive status with current_modulation 100 and checks the series
- [x] #4 The badge's accessible name contains its visible text; IDLE with anti_cycling reads 'Idle · anti-cycling pause'; the STOPPED sub-line does not ask to toggle the switch during a takeover
- [x] #5 home-heatpump-wording waits for the label/status to settle instead of a fixed sleep
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-25 (lane/ba-3012, stacked on lane/ba-3035): AC1 satOtLost/satOutsideMissing hidden when the state is paused_ot/paused_outside, still shown in STOPPED. AC2 .sat-chart-empty.corner moved top-right, clear of the y-axis labels; test compares the note rect with every SVG text rect in both themes (screenshots: OTGW-build-artifacts-safe/evidence-TASK-3012/after/paused-chart-light.png, -dark.png). AC3 satHistPush stores null for current_modulation and pid_output while active is false (drawn as a gap); test feeds active=false, current_modulation 100 and counts the modulation path. AC4 badge aria-label = visible text; IDLE + anti_cycling reads 'Idle · anti-cycling pause'; STOPPED during a takeover uses sat_sub_stopped_takeover (no switch toggle). Found and fixed while testing: the sub-line cache key used the state, now the text key, since STOPPED has two wordings. AC5 home-heatpump-wording waits for a per-case settle condition instead of sleep(800). EN, NL, DE, EL packs updated (sat_big_idle_anti_cycling, sat_sub_stopped_takeover; sat_msgs_aria removed). Evidence: tests/webui/sat-polish-3012.test.mjs red 0/5 on origin/dev data (note covers chart text, hint shown, modulation band drawn), green 5/5 on the fix; sat-display-states 5, home-heatpump-wording 3, sat-chart-actual-flow 4, i18n-lang 6, i18n-no-hardcoded 3, i18n-followups 4, sat-status-pill, sat-push-refused-hint, sat-explain-state, sat-hot-water-one-card, sat-weather-hint-link, sat-dhw-card, room-label-sat-source pass; evaluate.py --quick 88 pass 0 fail. Existing tests updated for the changed texts only: sat-display-states (new idle wording, badge name, OT case needs another message), sat-chart-actual-flow (fixture gets active true). Not done from the description: no hysteresis between WAITING and IDLE, STOPPED keeps the amber colour (P3, not in the ACs). Not verified: on a real unit; before-screenshots of the overlap (the red test message is the evidence); full webui suite on the NAS via 09.
<!-- SECTION:NOTES:END -->
