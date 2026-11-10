---
id: TASK-1779
title: >-
  Web UI: Hot water card shown twice (monitor/SAT view + settings), consolidate
  into one
status: To Do
assignee: []
created_date: '2026-11-10T14:27:00Z'
labels:
  - needs-maintainer-ok
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
- [ ] #1 Robert decided whether to keep both cards, merge them, or keep only one (and which)
- [ ] #2 If changed: the remaining card offers every control the removed one had, and a screenshot per language shows the result
<!-- AC:END -->
