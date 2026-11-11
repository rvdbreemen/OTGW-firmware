---
id: TASK-1779
title: >-
  Web UI: Hot water card shown twice (monitor/SAT view + settings), consolidate
  into one
status: Done
assignee:
  - '@claude'
created_date: '2026-11-10T14:27:00Z'
updated_date: '2026-11-11T05:12:00Z'
labels:
  - web-ui
milestone: 2.0.0
dependencies: []
ordinal: 875000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Suggestion from Sergeant D, #dev-sat-mqtt 2026-11-10 14:25:10 UTC, msg 1569704474744717437 (https://discord.com/channels/812969634638725140/1105556725714649128/1569704474744717437): "Do we really need both these cards? My recommendation is to keep only the first one. Actually they are repeating the same things". His two screenshots (attached to that message; local copies in the comms session scratchpad greekshots/shot11.jpg and shot12.jpg): card 1 = the Hot water card in the monitor/SAT view (standalone DHW control switch, DHW target temperature slider, hint); card 2 = the Hot water card with the switches 'DHW enabled (standalone)' and 'DHW enable (HW= master)' and a DHW temperature number field (30-60). A web UI layout change, so Robert decides.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Robert decided whether to keep both cards, merge them, or keep only one (and which)
- [x] #2 If changed: the remaining card offers every control the removed one had, and a screenshot per language shows the result
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 Robert decided (via the coordinator's AskUserQuestion): keep one card, consolidate into the first (the SAT Hot water card, Sergeant D's recommendation). Implementation (v2.js only): the SAT_L2 group ['Hot water', ['satdhwenabled','satdhwenable','satdhwsetpoint']] is removed and the three keys join SAT_NOT_IN_FORMS (otherwise a SAT key in no group lands in L3 'Other', see the comment above SAT_L2). The remaining card covers every control: satDhwEnabled (standalone switch), satDhwSlider (setpoint 30-60; 'below 30 = off' of the form equals the standalone switch off, TASK-1279), satDhwEnable (HW=, shown only with a DHW tank; the firmware acts on it only with a tank, SATcontrol.ino:4333-4336, so nothing is lost without one). Test tests/webui/sat-hot-water-one-card.test.mjs: 3/3 on the fix, 2 of 3 red on the previous v2.js. Also fixed the stale tests/test_sat_dhw_range.py assertion ('off' literal -> t('tools.sat_dhw_off'), stale since the TASK-1764 i18n; failed on origin/dev too). Screenshots per language (AC2) on the bench with the build.

2026-11-10 23:50 shipped in beta.173 (0d02754a5, pushed; zip msg 1569845393427009538). Bench .63: the served v2.js carries SAT_NOT_IN_FORMS {..., satdhwenabled:1, satdhwenable:1, satdhwsetpoint:1} and no 'Hot water' SAT_L2 group; the SAT Hot water card renders title, switch, slider and hint in en/nl/de/el. AC2 still open: the screenshots per language and a live look at the SAT settings form were not taken (Playwright MCP unavailable in this session; DOM dumps only render the Home view). tests/webui/sat-hot-water-one-card.test.mjs 3/3.

2026-11-11 04:2x AC2 on .63 (beta.174) with headless Chrome through the project's harness pieces (launchChrome slot + guarded tree; one Chrome, one page load per language and width; 0 otgw-cdp Chrome left): screenshots C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\shots-0911\t1779-hotwater-{en,nl,de,el}-{desktop,phone}.png (1280 and 375 px) show the single Hot water card (title, standalone switch, setpoint slider, hint). With the SAT settings forms rendered (82 [data-key^=sat] keys in the DOM) none of satdhwenabled/satdhwenable/satdhwsetpoint is present (report C:\Users\rvdbr\AppData\Local\Temp\claude\d9c572fbc7f8e9ee\4f6ad420-bf33-4a3a-88bc-4eb9890a2231\scratchpad\measure-0911\report.json): the removed group does not come back and the keys do not fall into 'Other'.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: Robert decided keep one card (the SAT Hot water card). AC2: shipped in beta.173 (0d02754a5), checked on .63 with beta.174: the card keeps every control (standalone switch, 30-60 slider, HW= switch with a tank, SATcontrol.ino:4333-4336), screenshots in en/nl/de/el at 1280 and 375 px, and the rendered SAT forms (82 sat keys) contain no satdhw* field. Test tests/webui/sat-hot-water-one-card.test.mjs 3/3, 2 of 3 red on the previous v2.js.
<!-- SECTION:FINAL_SUMMARY:END -->
