---
id: TASK-3033
title: >-
  Retained thermostat/override_refused flag: remember that the thermostat
  ignored a TT=/TC= override
status: To Do
assignee: []
created_date: '2026-11-22T10:33:00Z'
updated_date: '2026-11-22T19:35:00Z'
labels:
  - mqtt
  - feature
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 1142000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Tester request (gruijter, #alpha-testing 1574009591627513958, 2026-11-22). thermostat/sent_msgids shows 9R, but a thermostat that reads MsgID 9 may still ignore TT=/TC= (his Honeywell reads 9, not 100). OT has no refusal message. sat/push_refused (SATcontrol.ino:2507, 5026) is the only signal, reset on every push and only while SAT pushes TC=. Proposal: a retained thermostat/override_refused true/false, set when MsgID 16 did not follow a TT=/TC= within 5 min, so clients know beforehand whether an override works.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Maintainer decides whether and in which release
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22: Robert released it in #alpha-testing (1574014560933781527): 'This can go in after Wednesday.' Label needs-maintainer-ok removed; start after the budget reset (Wed 25 Nov).

2026-11-22: gruijter widened the proposal (#alpha-testing 1574018220942164069): retained lists in the supported_msgids style instead of one flag. boiler/ignored_msgids, e.g. 0W.6,14W,56W: DHW blocking (ID0 bit 6) while DHW stays active; ID14 max modulation while ID17 goes above it; ID56 when its read-back stays different. thermostat/ignored_msgids, e.g. 9R: TT=/TC= that ID16 does not follow within 5 min. Robert released the thermostat flag; the boiler list and the list format are new scope for his decision.

2026-11-22: Robert (#alpha-testing 1574019206737960982): 'The problem with this is also there is no way to read what is supported. The commands and the msgids. The only thing we can do is the administration of how boiler and thermostat respond.' Reading: any ignored list is observed behaviour (administration of responses), not a capability read; design accordingly. Boiler-list scope still undecided.

2026-11-22: Robert (#alpha-testing 1574019409008271423): 'Not sure if we should go beyond that, as there is a huge matrix of thermostats and boilers, that have different support.' Reading: keep scope to the released thermostat override flag; boiler ignored list leans no, not a final decision.

2026-11-22: gruijter's reason (#alpha-testing 1574020462642593843, answering Robert 1574019711568580692): for his Homey app to know which features the user's own system supports, so setup can skip parts that will not work; config options that later turn out not to work confuse users. Robert asked why the gateway should know this; decision on scope still his.

2026-11-22: will_85903 (#beta-testing 1574146013650751559) reports his Honeywell Round thermostat DOES receive the setpoint SAT pushes via MQTT (screenshot of the push setting enabled), addressed to gruijter, whose Honeywell ignores TT=/TC=. Same brand, different result: model/firmware variant matters; evidence that an observed 'override ignored' flag is per installation.
<!-- SECTION:NOTES:END -->
