---
id: TASK-2957
title: >-
  SAT page paused-state honesty: one status, paused markers, burner-cycle Off
  text (UX review F3 F7 F12)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-20T14:17:00Z'
updated_date: '2026-11-21T14:10:00Z'
labels:
  - ux
  - webui
  - needs-dev-input
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1091000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
From the TASK-2952 UX review (docs/reviews/2026-11-20_ux-heuristic-v2-webui/README.md, findings F3, F7, F12). Display only, no change to SAT control behaviour. F3: with SAT enabled and no OpenTherm connection the headline says 'At target · idle' (v2.js:1577) while the sub-line says 'SAT paused' (v2.js:572), the chip says Idle and a red '1' badge has no label. F7: paused Control status values (modulation 100 %, error, PID output) look live. F12: Off text 'SAT is not actively controlling. SAT chooses the mode itself.' contradicts itself (v2.js:1606-1608). needs-dev-input: otgw-dev delivers the state semantics first (which SAT states exist and when the UI should call it paused); otgw-webui builds after that. Evidence (local, not in git): D:\Users\Robert\Documents\GitHub\RvdB\OTGW-field-data\webui-screens\2026-11-20_ux-heuristic\ (scripts in tools\, run with OTGW_DATA_DIR=<tree>/src/OTGW-firmware/data). Files: sat-simple-1400-dark.png, sat-simple-1400-light.png, sat-control-1400-dark.png, mock-sat-control-1400-light.png.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 otgw-dev's answers Q1-Q7 are applied as in spec T2 section 6 (the builder checks the cited lines still match origin/dev)
- [ ] #2 Each display state of spec T2 section 2 (OFF, STOPPED, PAUSED_OT, PAUSED_OUTSIDE, TAKEOVER, WAITING plus its boiler-link-down variant, WINDOW, SUMMER, HEATING, IDLE) renders the pill, headline, sub-line and card treatment of section 3, checked by a browser test with one status object per state; during TAKEOVER the switch label reads 'Off · takeover active'
- [ ] #3 With enabled true, ot_link_lost true and boiler_status idle (the .63 case) the headline never shows 'At target' or 'idle'
- [ ] #4 The IDLE headline follows spec T2 section 3.1 (band at target + 0.1, 'above target' from + 1.0): room 25.5 / target 19.8 reads 'Idle · room above target', room 20.0 / target 19.8 'Idle · room at target'
- [ ] #5 While active is false Control status shows '—' for P, I, D, PID output, Error and Modulation; the Modulation row is labelled 'Modulation sent'; the Burner cycle card shows '—' in STOPPED, PAUSED_OT, PAUSED_OUTSIDE and WAITING even with control_mode 1 or 2
- [ ] #6 The messages badge shows visible text ('1 message' / 'N messages') and keeps its aria-label; the Burner cycle Off sub-line is 'SAT is not actively controlling.'
- [ ] #7 New EN keys exist, NL and DE through the i18n sync, tests/webui/i18n-*.test.mjs pass; sat_big_safety, sat_big_at_target, sat_cycle_sub_off and kv_modulation keep their names and get re-translated NL and DE values, visible in the sync diff
- [ ] #8 No change outside src/OTGW-firmware/data/ and its tests
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-20 otgw-ux-designer: spec docs/reviews/2026-11-20_ux-heuristic-v2-webui/spec-T2-sat-display-states.md (display states, copy EN, edge cases, Q1-Q7 for otgw-dev). ACs replaced by the spec's acceptance list. Correction to review F3: the messages badge already has aria-label 'Messages: N' (v2.js:7293); only visible text is missing.

2026-11-20 otgw-ux-designer: otgw-dev answered Q1-Q7 (relayed by the coordinator); spec T2 updated: STARTING became WAITING (unbounded gates, no time promise) with a boiler-link-down variant; IDLE uses the firmware band (target + 0.1); not-live values show '—'; current_modulation is the MM SAT sends, so the row is 'Modulation sent'; the burner cycle card ignores control_mode while not active. Follow-up proposal for otgw-dev (not in this task): a wait_reason field in /v2/sat/status.

Suggestion (not in this task, no separate task yet, coordinator 2026-11-20): a wait_reason field in /v2/sat/status (boot_silence, ntp, no_interface, no_boiler, mqtt_fallback, flashing) so the WAITING state can name its reason; owner otgw-dev if picked up.

2026-11-21 otgw-webui (WIP, paused for TASK-2998; uncommitted in wt-ba-2959 on lane/ba-2957): satDisplayState() per spec T2 section 2, pill/headline/sub-line/stale cards per section 3, IDLE per 3.1, dashes while not active, Modulation sent, burner cycle per 3.3, visible messages badge text, loop label state word, takeover switch label. Firmware lines still match origin/dev: bActive=true SATcontrol.ino:8045, bSafetyTripped=true :8536, SAT_MAX_PIC_FAILS :148. New test tests/webui/sat-display-states.test.mjs: RED on the old code 0/3 (headline 'At target · idle' for enabled+ot_link_lost, the .63 case), GREEN 3/3. Also fixed t(): a count key without plural forms returned the bare key, so the messages aria-label read 'tools.sat_msgs_aria' and the connection summary 'conn.degraded'; now falls back to the plain key. Updated sat-status-pill (safety trip pill Stopped, inactive = Waiting) and sat-explain-state (badge text) to the spec. NL/DE/EL written by hand per i18n/glossary.md (32 keys each; no review workflow run, EL needs a native read). Green: sat-explain-state, sat-status-pill, sat-page-order, i18n-lang 6/0, i18n-no-hardcoded 3/0, i18n-followups 4/0, lang-menu 7/0. Screenshots before/after (paused_ot, waiting, takeover, idle_above, light+dark) in OTGW-build-artifacts-safe/evidence-TASK-2957/shots/. Open: full webui sweep, build, bench, push after the 20:00 compose (coordinator).

2026-11-21: fixed 'Set up a source': it opened Settings › SAT, which only shows the 'moved to the SAT page' card (TASK-1211); it now opens the SAT page in the Control view and focuses Latitude (the Weather group heading while SAT is off). New case in sat-display-states.test.mjs: red on the previous commit (page not SAT), green now; file 4/0.

2026-11-21 ux critique P1 (otgw-ux-designer): 'Check connection ›' called showPage('monitor') without a tab, so Monitor restored its last tab (Log hides the map). Now showPage('monitor', {tab:'mconn'}). New case in sat-display-states: red on 8a5a226b6 (tab mlog), green now (file 5/0). NAS full sweep on 9d3056538 (otgw-bench-tester): 91/101 first pass, 6 of 10 green on rerun; contrast-tokens, inline-accent-contrast, harness-stall, poll-spread-hidden red only by CDP evaluate/timing timeouts on the NAS (no assertion), all green locally single-file.
<!-- SECTION:NOTES:END -->
