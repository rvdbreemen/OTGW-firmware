---
id: TASK-1884
title: >-
  Web UI SAT: explain the state with data the firmware already sends (why-line,
  messages panel, fallback text, trend arrow, curve advice, 24 h cycles)
status: To Do
assignee: []
created_date: '2026-11-13T06:23:00Z'
labels:
  - webui
  - sat
dependencies:
  - TASK-1879
priority: medium
ordinal: 957000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
UI proposals U1-U6 from the VTherm UI review (docs/sat/vtherm-comparison-2026-11-13.md, section Web UI). UI only (v2.js, v2.html, v2.css, i18n packs); no firmware change, no extra API call: everything reads the existing GET /api/v2/sat/status poll. U1 fallback_reason as text instead of a number (v2.js:7258, enum SATtypes.h:61). U2 a why-line under #satStatusBig from existing fields in a fixed priority: fallback_active/fallback_reason, pwm_veto_reason (1 no valid reply yet, 2 MsgID 3 bit 4 boiler forbids low-off), mod_suppressed, dhw_active, valves_open; texts describe what the port does and are checked against sergeant-d-corrections.md before merge. U3 an info/warning icon with an overlay listing all active messages, absorbing the four hint lines (v2.js:7067-7071). U4 a trend arrow next to the room temperature from indoor_rise_rate. U5 curve_recommendation (insufficient/increase/decrease/hold) on the heating-curve card. U6 a 24 h cycles card in the Technical view (24h_cycles, 24h_duty_ratio, 24h_overshoot_fraction, 24h_underheat_fraction, 24h_long_cycle_fraction). Constraints: ADR-091 class drift gate, t() for every string (nl/de/el via translate_webui_otgw-firmware), max 2 calls in flight, LittleFS budget.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 tests/webui test(s) cover U1-U6 rendering from a fixture sat/status payload; red before the change, green after
- [ ] #2 python evaluate.py --quick shows no new FAIL (ADR-091 drift gate included); i18n-no-hardcoded and the other webui tests green
- [ ] #3 No new fetch: the SAT page still issues only sat/status + sat/weather per poll (network capture or test)
- [ ] #4 nl/de/el packs carry the new keys (translate skill sync mode)
- [ ] #5 Bench screenshot of the SAT page on a unit (otgw-bench-tester) showing the why-line and the messages panel
<!-- AC:END -->
