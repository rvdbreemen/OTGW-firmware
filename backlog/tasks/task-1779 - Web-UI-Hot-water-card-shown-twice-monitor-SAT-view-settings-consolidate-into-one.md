---
id: TASK-1779
title: >-
  Web UI: Hot water card shown twice (monitor/SAT view + settings), consolidate
  into one
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T14:27:00Z'
updated_date: '2026-11-10T18:18:00Z'
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
- [ ] #2 If changed: the remaining card offers every control the removed one had, and a screenshot per language shows the result
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-10 Robert decided (via the coordinator's AskUserQuestion): keep one card, consolidate into the first (the SAT Hot water card, Sergeant D's recommendation). Implementation (v2.js only): the SAT_L2 group ['Hot water', ['satdhwenabled','satdhwenable','satdhwsetpoint']] is removed and the three keys join SAT_NOT_IN_FORMS (otherwise a SAT key in no group lands in L3 'Other', see the comment above SAT_L2). The remaining card covers every control: satDhwEnabled (standalone switch), satDhwSlider (setpoint 30-60; 'below 30 = off' of the form equals the standalone switch off, TASK-1279), satDhwEnable (HW=, shown only with a DHW tank; the firmware acts on it only with a tank, SATcontrol.ino:4333-4336, so nothing is lost without one). Test tests/webui/sat-hot-water-one-card.test.mjs: 3/3 on the fix, 2 of 3 red on the previous v2.js. Also fixed the stale tests/test_sat_dhw_range.py assertion ('off' literal -> t('tools.sat_dhw_off'), stale since the TASK-1764 i18n; failed on origin/dev too). Screenshots per language (AC2) on the bench with the build.
<!-- SECTION:NOTES:END -->
