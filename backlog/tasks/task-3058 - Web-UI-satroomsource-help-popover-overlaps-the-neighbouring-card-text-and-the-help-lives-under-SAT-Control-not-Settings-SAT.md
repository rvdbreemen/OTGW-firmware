---
id: TASK-3058
title: >-
  Web UI: satroomsource help popover overlaps the neighbouring card text, and
  the help lives under SAT Control, not Settings > SAT
status: To Do
assignee: []
created_date: '2026-11-25T12:27:00Z'
updated_date: '2026-11-27T05:20:00Z'
labels:
  - webui
  - ux
milestone: 2.1.0
dependencies: []
ordinal: 1158000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Bench screenshot S22 (OTGW-field-data/bench-63/2026-11-25_webui-431/screenshots/S22-light-1400-help.png, build 3d0df2d): the (i) popover of Room temperature source (satroomsource) overlaps the text of the neighbouring card. The setting is reached at SAT page > Control > SAT settings > Indoor temperature; Settings > SAT only shows Open SAT settings. Not a beta blocker: the text is complete and not cut off (scrollHeight = clientHeight).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The help popover of a SAT setting never overlaps the text of the neighbouring card at 1400 and 390 px, light and dark
- [ ] #2 Browser test with the popover open and screenshot as evidence
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-27 otgw-ux-designer, diagnosis from bench screenshot S22-light-1400.png (OTGW-field-data/bench-63/2026-11-25_webui-431): the popover #shelp is position:fixed, max-width 320 px, placed by helpPlace() (v2.js ~4652) with left = icon left clamped to the VIEWPORT only. The icon of 'Room temperature source' sits in the middle card, so the popover runs over the Zones card to its right and hides its labels. A floating popover always covers something; the rule that fits the AC is: it may cover its own card (the card it explains), never the next one. CSS cannot do this (the box is fixed to the viewport), so it is a v2.js change for otgw-webui. Exact change in helpPlace(): before measuring set a card-limited width, then clamp left to the card: var card = helpBtn.closest('.set-group'); var cr = card ? card.getBoundingClientRect() : null; var minL = cr ? Math.max(m, cr.left + 8) : m; var maxR = cr ? Math.min(vw - m, cr.right - 8) : vw - m; helpPop.style.maxWidth = Math.max(160, maxR - minL) + 'px'; helpPop.style.left = '0px'; (measure w, h as now); helpPop.style.left = Math.max(minL, Math.min(r.left, maxR - w)) + 'px'. Icons outside a .set-group (card is null) keep today's behaviour. Test: tests/webui, fake device with SAT settings, hover the satroomsource icon at 1400 and 390 px, assert popover left >= card.left and right <= card.right; red on the old v2.js. Not verified: I have not run this change (v2.js belongs to otgw-webui); the 'help' crop screenshots in the bench folder are blank, so the diagnosis rests on S22-light-1400.png alone.

2026-11-27 otgw-ux-designer: released to otgw-webui by coordinator decision (small v2.js change; the exact change above is the spec). I do not touch v2.js. He claims the task.
<!-- SECTION:NOTES:END -->
