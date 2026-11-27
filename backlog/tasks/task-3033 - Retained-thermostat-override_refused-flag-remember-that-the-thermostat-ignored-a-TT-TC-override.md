---
id: TASK-3033
title: >-
  Retained thermostat/override_refused flag: remember that the thermostat
  ignored a TT=/TC= override
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-22T10:33:00Z'
updated_date: '2026-11-27T05:43:00Z'
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
- [x] #1 Maintainer decides whether and in which release
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
PLAN (otgw-poc-dev, 2026-11-27; nothing built, nothing run on a unit). Scope per Robert 2026-11-22 (released: the thermostat flag only, boiler list leans no): one retained flag, observed behaviour, not a capability read.
Facts: satPushRefusedDecide (SATcontrol.ino:2565-2573, TASK-1343) already judges 'T frame of MsgID 16 differs >=300 s after a TC= push' but only for SAT's push (bTCAccountedValid, iTCPushMs, fLastSentTC) and only while push setpoint is on; published as sat/push_refused (SATcontrol.ino:5469, retained). It is called from satOnThermostatSetpoint on every MsgID 16 T frame. TT=/TC= from MQTT/REST/telnet carry no timestamp or value today (only SAT's own TC= is tracked), so the new flag needs those two numbers for external overrides too.
Files: (1) OTGW-Core.ino addCommandToQueue (:3991, sees every TT=/TC= with satOwn): record fOverrideSent + iOverrideMs for a non-zero TT=/TC= (0 = cancel clears the pending check). (2) SATcontrol.ino or OTGW-Core.ino next to satOnThermostatSetpoint: reuse satPushRefusedDecide as is (same 0.25 C and 300 s rule, so both flags agree); the verdict is only a state flag (processOT holds the OT lock, no MQTT there). (3) MQTTstuff.ino: publish otgw-firmware/thermostat/override_refused true/false, retained, via the same on-change helper pattern as sent_msgids; NOT tied to satIsEnabled (gruijter needs it with SAT off). (4) docs/api/MQTT.md row + manual line. (5) state in the types header, reset on boot and on MQTT reconnect republish.
Topic name: otgw-firmware/thermostat/override_refused (next to thermostat/sent_msgids). HA discovery: none proposed. sat/push_refused has no discovery entry either (grep MQTTstuff.ino: no hit); a tri-state (unknown until a first override is judged) does not map to a binary_sensor. Consumer is Homey over MQTT. I ask only if Robert wants a diagnostic binary_sensor.
Open design points to put to the coordinator: (a) before the first override since boot the flag is NOT published (an empty retained value would lie, same lesson as TASK-1781); (b) a gateway reset also clears an override (MsgID 9 reads 0 after the PIC clears it), which entry 16 already names as indistinguishable from a refusing thermostat: the flag stays 'observed after 300 s', a PIC reset inside the window can read as refused; label it in the docs; (c) persistence across reboot: not persisted, it is re-learned at the next override.
Host test, red first (real functions compiled, like tests/test_sat_push_refused.py): thermostat T frame stays at its own value 300 s after an external TT=/TC= -> true; follows within 0.25 C -> false; TC=0 clears; value inside 300 s keeps the old verdict; SAT's own push still drives sat/push_refused unchanged. Red on origin/dev (no flag/topic), green on the fix; mutants: 300 s -> 30 s, 0.25 -> 2.5, retained false.
Entries: 14 and 16 (push on target change, no re-push; 16 changes 14 and states the PIC-reset ambiguity) are touched in reading only: no SAT control behaviour changes, status only like entry 16 and TASK-1343. ADR-223 (TT=/TC= no longer replace MsgID 16, so a T frame is the thermostat's own value) is what makes the check valid. No new Sergeant D entry needed; I name that and ask you if you read it otherwise. Not verifiable on host: the thermostat itself; the bench has none, a field tester (gruijter's Honeywell ignores TT=/TC=, will_85903's follows) closes the evidence.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-22: Robert released it in #alpha-testing (1574014560933781527): 'This can go in after Wednesday.' Label needs-maintainer-ok removed; start after the budget reset (Wed 25 Nov).

2026-11-22: gruijter widened the proposal (#alpha-testing 1574018220942164069): retained lists in the supported_msgids style instead of one flag. boiler/ignored_msgids, e.g. 0W.6,14W,56W: DHW blocking (ID0 bit 6) while DHW stays active; ID14 max modulation while ID17 goes above it; ID56 when its read-back stays different. thermostat/ignored_msgids, e.g. 9R: TT=/TC= that ID16 does not follow within 5 min. Robert released the thermostat flag; the boiler list and the list format are new scope for his decision.

2026-11-22: Robert (#alpha-testing 1574019206737960982): 'The problem with this is also there is no way to read what is supported. The commands and the msgids. The only thing we can do is the administration of how boiler and thermostat respond.' Reading: any ignored list is observed behaviour (administration of responses), not a capability read; design accordingly. Boiler-list scope still undecided.

2026-11-22: Robert (#alpha-testing 1574019409008271423): 'Not sure if we should go beyond that, as there is a huge matrix of thermostats and boilers, that have different support.' Reading: keep scope to the released thermostat override flag; boiler ignored list leans no, not a final decision.

2026-11-22: gruijter's reason (#alpha-testing 1574020462642593843, answering Robert 1574019711568580692): for his Homey app to know which features the user's own system supports, so setup can skip parts that will not work; config options that later turn out not to work confuse users. Robert asked why the gateway should know this; decision on scope still his.

2026-11-22: will_85903 (#beta-testing 1574146013650751559) reports his Honeywell Round thermostat DOES receive the setpoint SAT pushes via MQTT (screenshot of the push setting enabled), addressed to gruijter, whose Honeywell ignores TT=/TC=. Same brand, different result: model/firmware variant matters; evidence that an observed 'override ignored' flag is per installation.

2026-11-27 otgw-poc-dev implemented on lane-poc-3033 (based on lane-poc-3016, bundle order 3032, 3016, 3033; coordinator OK 27 Nov). AC1: Robert released it 2026-11-22 (#alpha-testing 1574014560933781527), scope = the thermostat flag only.

Evidence (host): tests/test_ot_override_refused_3033.py compiles the real otOverride block from OTGW-Core.ino: 8/8 green; against origin/dev OTGW-Core.ino red (block missing, setUpClass error); 5 mutants killed (300 s -> 30 s, 0.25 -> 2.5, cancel resets verdict, no 'equal to own setpoint' guard, no dirty mark). Neighbours green: test_mqtt_ot_support_lists 10/10 (stub extended with the new list bit), test_sat_push_refused 6/6; evaluate.py --quick 89 pass / 0 fail. Wiring (call sites, topic, reconnect republish, docs row) is a TEXT check, not a run. NOT run: the true/false payload branch of publishOtSupportMsgidLists (the stub returns verdict 0), the broker, any thermostat. Needs a field thermostat to close (gruijter's Honeywell should read true after 5 min, will_85903's false); no bench unit has a thermostat.

Design as built: topic otgw-firmware/thermostat/override_refused true/false retained; carried by the existing support-list dirty-bit mechanism (OT_SUPPORT_LIST_OVERRIDE = 4: minute tick plus reconnect republish); published only after the first judgement; not persisted. Every TT=/TC= counts (also SAT's own, also SAT off); TT=0/TC=0 cancels, judges nothing, verdict kept; an override equal to the thermostat's own last setpoint is not judged. Separate from sat/push_refused, which is unchanged. Known limit (entry 16): a gateway reset clears the override and can read as refused inside the 300 s window; documented in docs/api/MQTT.md. Entries 14, 16 and ADR-223 read; no control behaviour changed. ACs 2+ do not exist in the task: only AC1; leaving the task In Progress until bench/field evidence.
<!-- SECTION:NOTES:END -->
