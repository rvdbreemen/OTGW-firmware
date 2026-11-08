---
id: TASK-1735
title: >-
  Web UI: temperature controls only with SAT on; with SAT off the home view only
  shows the thermostat's values (supersedes TASK-1717 TT= dial)
status: Done
assignee: []
created_date: '2026-11-08T15:30:00Z'
updated_date: '2026-11-08T21:27:00Z'
labels:
  - webui
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 842000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-08 ~16:30: 'SAT enabled: the UI is responsive, the set temperature is for SAT, and SAT sets the boiler values. Period. SAT disabled: the UI only shows what the real thermostat has set, nothing else.' and 'The UI may only be controllable or settable when SAT is enabled.' The OT-Thing controller is already removed (TASK-1684). Reverses TASK-1717 (beta.146, bc8f07818: dial sends TT= with SAT off). Scope: with SAT off the dial +/- (and any other setpoint control on the home/phone view) is hidden or disabled with a short note ('SAT is off: the thermostat controls the temperature'); target shows the thermostat's room setpoint (MsgID 16), big number the room temperature (MsgID 24); no TT= from the UI. With SAT on unchanged (SAT target).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 SAT off: no control element on the home view sends anything; dial shows thermostat values only (webui test red/green)
- [x] #2 SAT on: dial sets the SAT target as before (test)
- [x] #3 Bench .61 with SAT off and on, screenshots
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-08 implementation (Robert, quoted above; confirmed on Discord 15:39 'the dial is only for SAT'). v2.js: the TT= code of TASK-1717 (stepRoomOverride) removed; applyDialMode() hides bUp/bDown and shows 'SAT is off: the thermostat controls the temperature' when sat/status says enabled and fallback_active are false (called from fetchSatStatus and from stepTarget); stepTarget() itself returns without any request when SAT does not own the temperature, so a script click on the hidden + sends nothing (tip from Geheugengebruik, covered by the test). Dial values unchanged: target = thermostat room setpoint MsgID 16, big number = room temperature MsgID 24. Only home-view setpoint control is the design b dial; the SAT page's own target controls are SAT configuration and stay. Test tests/webui/home-dial-sat-only.test.mjs (git mv of the 1717 test): fix 2/2 pass; origin/dev v2.js: the SAT-off test fails (buttons shown, TT= sent), the SAT-on guard passes. evaluate --quick 0 failed. Bench .61 (beta.148 firmware + this v2.js via ota_flash --webui): first attempt at ~16:40 aborted: .61 answered every REST call 503 'low heap' (max free block 7668 B < 8192 gate, filed as TASK-1736 by the coordinator); the --webui push failed, so the old 1717 page sent TT= (cleared by the reboot). Reboot via /ReBoot (coordinator OK) 16:51: boot heap 86608 B, max free block 42996 B, TT overrides gone. Redo: SAT off -> + and - hidden, forced clicks on both POSTed nothing, message shown, overrides empty, sat_owns false (screenshot scratchpad bench1681/d1735-satoff.png); SAT on -> + and - shown, + POST sat/target 20.5, - POST 20 (target restored to 20), 'target set 20.0°' (d1735-saton*.png). Settings 164 -> 164 diff none, SAT enabled+active, crashlog empty.

2026-11-08 17:3x, Robert in #dev-sat-mqtt 17:23-17:27 (relayed by the coordinator): the dial never sends TT=, only the SAT target; UI control only with SAT on; with SAT off only the thermostat's values and the room temperature; with SAT on PIC commands are blocked and SAT is controlled through its own MQTT/REST. Checked: every home view shows only model.room (MsgID 24) and model.roomSet (MsgID 16) with SAT off: design a aRoom/aRoomSet (v2.js:1119-1125, SAT's target only while sat.active), design b bRoom/bSet and the arc/tick (1199-1216), design c Room chip and T ROOM/ROOM SET (1176, 1319-1320); no derived value. Both sources are the thermostat's: WS frames MsgID 24/16 and /api/v2/otgw/otmonitor roomtemperature = Tr (OTGW-Core.ino:5047, MsgID 24) and roomsetpoint = TrSet (5036, MsgID 16), restAPI.ino:3068-3069. New test case: SAT off, otmonitor 19.8/20.0 -> bRoom '19.8°', bSet 'target 20.0°'; the test file now passes 3/3. SAT settings pages unchanged pending Robert's answer on whether they stay editable with SAT off.

2026-11-08 review (OTGW agent 3) and Robert's takeover rule (via the coordinator): during a SAT fallback takeover (fallback_active, Enable switch off) the dial stays on and sets the SAT target, because SAT really controls (sat.owns = enabled || fallback_active); the SAT settings stay locked then (TASK-1737). Presses while sat.owns is still unknown share one sat/status ask (dialAsk guard, presses add up), so the 2-in-flight rule holds. Tests added to home-dial-sat-only.test.mjs: takeover (+/- shown, POST sat/target 20.5) and four presses with a 1.5 s late sat/status give one ask and no POST. The SAT-off test now waits until design b is visible and the status is applied; before, under load it could read a zero-width dial as hidden and an empty command line (flaky 1 in 4).

2026-11-08 17:23-17:27 Robert in #dev-sat-mqtt (msgs 1569024462718894322..1569025571319586898): 'the dial should never just send a TT command. The dial is for controlling SAT desired room temperature.' / 'The whole UI that just shows a dial and all kinds of settings is related to SAT and can only by used when SAT is enabled by the user.' / 'When SAT is turned off it should not be controllable. It is allowed to show the current value of a real thermostat ... and the room temperature if that is available.' / SAT off: remote control via MQTT, REST, serial stays; SAT on: PIC commands blocked, SAT topic on MQTT/REST. Discord agent answered (msg in #dev-sat-mqtt) and asked whether SAT settings pages stay editable with SAT off (recommend: yes, lock only the dial); answer pending. (carried over from the main dev tree copy by otgw-firmware-4c, 2026-11-08)
<!-- SECTION:NOTES:END -->
