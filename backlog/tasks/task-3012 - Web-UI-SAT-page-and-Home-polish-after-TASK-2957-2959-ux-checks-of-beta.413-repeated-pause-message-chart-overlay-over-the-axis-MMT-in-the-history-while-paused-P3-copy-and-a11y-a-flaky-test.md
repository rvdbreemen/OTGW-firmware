---
id: TASK-3012
title: >-
  Web UI: SAT page and Home polish after TASK-2957/2959 (ux checks of beta.413):
  repeated pause message, chart overlay over the axis, MM=T in the history while
  paused, P3 copy and a11y, a flaky test
status: To Do
assignee: []
created_date: '2026-11-21T15:44:00Z'
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
- [ ] #1 In PAUSED_OT and PAUSED_OUTSIDE the matching hint line is not shown in the messages panel (the headline and sub-line carry it); it still shows when the state does not cover it
- [ ] #2 The control-loop chart note does not overlap the axis labels in any state (screenshot light and dark)
- [ ] #3 While active is false no modulation or PID value is added to the SAT history (drawn as a gap); a browser test feeds an inactive status with current_modulation 100 and checks the series
- [ ] #4 The badge's accessible name contains its visible text; IDLE with anti_cycling reads 'Idle · anti-cycling pause'; the STOPPED sub-line does not ask to toggle the switch during a takeover
- [ ] #5 home-heatpump-wording waits for the label/status to settle instead of a fixed sleep
<!-- AC:END -->
