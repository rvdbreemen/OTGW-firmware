---
id: TASK-1893
title: >-
  Web UI connection map: OT bus runs Thermostat -> OTGW -> Boiler as a chain;
  MQTT and live links leave the router from top and bottom centre
status: Done
assignee:
  - '@claude'
created_date: '2026-11-13T09:06:00Z'
updated_date: '2026-11-13T16:06:00Z'
labels:
  - webui
dependencies: []
priority: medium
ordinal: 964000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert, 2026-11-13 (screenshot of Monitor > Connection): (1) the green MQTT and live lines end on the right edge of the Router box; they are two different links and should leave the Router at the middle of its top and bottom edge. (2) The OT bus is interrupted by the gateway: the red line runs Thermostat -> OTGW and OTGW -> Boiler, not Thermostat-to-Boiler with a branch to the OTGW; Thermostat, OTGW and Boiler stand in one line. Static SVG geometry in data/v2.html (#connMap); no JS logic change.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 MQTT and live links start at the centre of the Router box's top and bottom edge
- [x] #2 OT bus drawn as two segments, Thermostat -> OTGW and OTGW -> Boiler, with the three nodes in one column
- [x] #3 connection-map-text-fits and the other webui tests that read the map pass; before/after screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 d3, lane-connmap (worktree wt-connmap), uncommitted until a bump slot: (1) #connMap: viewBox 610x372, Thermostat/OTGW/Boiler in one column, OT bus as two segments M115,80 V130 and M115,250 V300, Wi-Fi M210,190 H340, Router at 340..480, MQTT M410,160 V104 H540 and live M410,220 V300 H540 from the Router's top/bottom centre; override badge beside the upper bus segment. (2) Robert's follow-ups on the same screen family: Home schematic room readout moved clear of the flow-pipe corner (translate 580,122; value, setpoint beside it, ROOM tag under the value like OUTSIDE); SPACE HEATING centred under the emitter (x=638, clear of the return pipe at x=560). (3) Footer '© 2021-2026 Robert van den Breemen' removed; Settings gets an About card (site text: Robert, Schelte Bron, George Ntellas, Alex (Alexwijn), Nodo Shop, links) and a Contributors card: GitHub commits on dev/main/otgw-1.x.x (git shortlog, bots and AI agents out, aliases merged) and Discord messages since 2021 (59 channels/threads, 47980 human messages, bots and deleted accounts out), most first; strings EN/nl/de/el.

Evidence: screenshots before/after in d3 scratchpad connmap/ (before.png, after.png, schem_before.png, schem_after.png, about.png). Webui tests, every file one by one: connection-map-text-fits 3/3, health-chain-one-in-flight 2/2, i18n-no-hardcoded 3/3, i18n-lang 6/6, footer-copyright 2/2 (rewritten: footer gone, About card with Robert first then the site credits, Contributors card after it), v2-undefined-names 2/2 after npm ci; only failures graph-room-dropout, home-dial-sat-only, pic-flash-status, which fail on the base commit too (routed to 09).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Shipped in 2.0.0-beta.224 (5ddcc6543, post 1570832038179905578). AC1/AC2: connection map redrawn in data/v2.html (OT bus Thermostat -> OTGW -> Boiler in one column, MQTT and live from the Router's top and bottom centre); plus Robert's follow-ups on the same request: Home room readout and SPACE HEATING clear of the pipes, footer copyright moved to a Settings About card (site credits) with a Contributors card (GitHub commits on dev/main/otgw-1.x.x, Discord messages since 2021, most first). AC3: webui tests one file at a time all green (connection-map-text-fits 3/3, footer-copyright 2/2 rewritten, i18n-lang 6/6, i18n-no-hardcoded 3/3); before/after screenshots in d3 scratchpad connmap/; bench by 09 on 224: screenshots OK in light and dark and at 375 px. Not done: the Contributors list is a snapshot of 2026-11-13 and is not refreshed automatically.
<!-- SECTION:FINAL_SUMMARY:END -->
