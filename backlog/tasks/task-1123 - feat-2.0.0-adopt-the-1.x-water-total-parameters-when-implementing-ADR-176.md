---
id: TASK-1123
title: 'feat-2.0.0: adopt the 1.x water-total parameters when implementing ADR-176'
status: Done
assignee:
  - '@claude'
created_date: '2026-09-04 06:30'
updated_date: '2026-09-30 18:42'
labels:
  - 2.0.0
  - parity
dependencies: []
priority: medium
ordinal: 274000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
ADR-176 is Accepted on this line but nothing implements it: no source file under src/ references dhw_water_total, WaterTotal or waterTotal. The 1.x peer shipped in v1.7.5 (TASK-1091, TASK-1093, ADR-093, ADR-094), so the 1.x line is now the reference implementation rather than a parallel plan.\n\nTASK-1094 on the 1.x line compared the two and found the entity contract and the accumulation method identical, with the divergences confined to storage, which ADR-176's own Decision Contract already unbinds ('Storage mechanism is explicitly not bound by that mandate').\n\nTwo things the implementer needs to know, neither of which can be fixed by editing ADR-176, because it is Accepted and therefore immutable.\n\nFirst, the interval cap. ADR-176 states the rule ('the usable interval capped so a long sampling gap cannot invent litres') without a number. The 1.x line chose 60000 ms (dhwWaterMeter.ino:38, DHW_METER_MAX_GAP_MS), matching the firmware's own 60 s publish cadence, and accumulates flow * dtMs / 60000. Adopting a different number would make the two firmwares report different totals for the same boiler, which is exactly what ADR-090 Must #2 forbids.\n\nSecond, a premise in ADR-176 changed after it was accepted. Its answered question on partial regression after an unclean reboot states 'Same answer as the 1.x peer', which was true on 2026-08-24 under ADR-090. ADR-093 then superseded ADR-090 and removed persistence from 1.x entirely: the 1.x counter lives in RAM, is not persisted, and has no reset surface, because a reboot zeroes it. So 2.0.0 persisting while 1.x does not is a real difference in the number a user sees after a reboot, deliberate on both lines but no longer symmetrical the way ADR-176 assumed.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 The interval cap is 60000 ms, matching dhwWaterMeter.ino:38 on the 1.x line, and accumulation is flow * elapsed / 60000 so both firmwares report the same total for the same boiler
- [x] #2 The entity contract matches 1.x exactly: device_class water, unit L, state_class total_increasing, published only after a MsgID 19 frame has decoded
- [x] #3 The divergence from 1.x on persistence and on the reset surface is restated in the implementing task or a new decision record on this line, rather than by editing the Accepted ADR-176
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation (alpha.393)
- dhwWaterMeter.ino (new): updateDHWWaterMeter() follows the 1.x rule step for step. The first sample only seeds. dt = now - last (unsigned, wraps), and last is moved before the gap test. dt > DHW_METER_MAX_GAP_MS (60000 ms, the value at 1.x dhwWaterMeter.ino:38) adds nothing, a flow <= 0 adds nothing, otherwise total += flow * dt / 60000.
- Write sites: print_f88() for boiler Read-Ack frames that pass the validity gate, and updatePSSummaryFloatState() for the PIC PS=1 summary.
- Persistence: /dhw_water.json, written when >= 10 L are unsaved or 15 min after the last write with something unsaved, flushed in doRestart(), loaded in setup() after the LittleFS mount. No write while flashing or while LittleFS is unmounted.
- Discovery: faux id 241 (OTGWdhwmeterid), table row 389, queued at boot by queueNonOTDiscoveryIds() (ADR-176 Must #7). The 60 s task publishes the state only after a MsgID 19 sample on this boot.
- Reset: POST /api/v2/otgw/reset_water_total and MQTT set/<node>/otgw/reset_water_total only queue a flag. loop() then zeroes RAM and file together and publishes 0.0. A reset that arrives as a retained MQTT message (properties.retain) is ignored.

Divergences from the 1.x line (AC#3). 1.x was read at origin/otgw-1.x.x 8ba6f7ef9. These are restated here rather than in the Accepted ADR-176.
1. Persistence. 1.x keeps the total in RAM only (dhwWaterMeter.ino:30-40, ADR-093), so a reboot zeroes it. This line persists it in /dhw_water.json (ADR-176). An orderly restart publishes the same total again. A power cut restores up to 10 L plus one minute of flow less than the last published value. At 90% of the previous value or more, Home Assistant logs a dip and subtracts those litres from the statistic. Below 90%, which a drop of under 10 L reaches only while the total is below 100 L, it counts a meter reset and adds the restored total again (docs/api/MQTT.md, DHW Water Total). ADR-176's answered question on partial regression says "The counter never decreases" and "Same answer as the 1.x peer". Neither holds any more.
2. Reset surface. 1.x has none: a reboot is its only reset. This line has POST /api/v2/otgw/reset_water_total and set/<node>/otgw/reset_water_total (ADR-176 answered question 6). Refinement beyond ADR-176's text: a reset that arrives as a retained MQTT message is ignored. Under MQTT 3.1.1 (3.3.1.3) a broker sets the retain flag only on the copy it hands out for a new subscription, so a live publish still resets once, and a retained copy cannot zero the total again on every reconnect. A reset published with the retain flag while the gateway is offline does nothing.
3. Announce timing. 1.x announces discovery just in time, on the first MsgID 19 decode (MQTTstuff.ino publishDHWWaterMeter(), ADR-093). This line announces at boot like its other faux ids (ADR-176 Must #7), so the entity exists, with an unknown state, on installations that never see MsgID 19. The state rule is the same on both lines: nothing is published before a MsgID 19 frame decodes on this boot.
4. Samples counted. 1.x counts only print_f88() frames that pass validForMaster (OTGW-Core.ino:2082), so a 1.x PIC gateway in PS=1 mode counts nothing. This line also counts the PIC PS=1 summary line. On both lines, frames the gateway builds itself do not count: PIC answer overrides on both, and OT-Direct master-mode cache replays on this line only.
5. Flow bound. This line drops a reading that is not a number, is infinite, or is above 128 L/min. Only a malformed PS=1 summary field can deliver such a value. 1.x needs no bound: its only source is the f8.8 decode, which is always finite and below 128.
6. Identity. The faux id is 241 here and 243 on 1.x. The uniq_id is <nodeId>-sensors_dhw_water_total here (this line's device-prefixed scheme) and <nodeId>-dhw_water_total on 1.x. Moving a gateway from 1.x to 2.0.0 therefore gives Home Assistant a new entity.
7. Arithmetic and formatting. The total is a double here and a float on 1.x. Both publish one decimal: %.1f into a 24-byte buffer here, dtostrf into 16 bytes on 1.x.

Entity contract (AC#2), identical on both lines: label dhw_water_total, name DHW_Water_Total, device_class water, unit L, state_class total_increasing, icon water, entity_category none, enabled. The state is not retained. It is published only after a MsgID 19 frame has decoded on this boot. Compared: 1.x mqtt_configuratie.cpp:66, :653 and :1130 with this line's MQTTHaDiscovery.cpp:354 and :1365.

Evidence (collected this session on the main tree, HEAD 7f636ff42 plus this change)
- python test/host/test_dhw_water_meter.py --old-rev HEAD: RESULT: PASS, 117 verdict checks yes, 0 NO.
- AC#1 cases: U1 (time-based: 3.0 L sampled every 10 s or every 5 s), U2 (a 60000 ms interval counts, 60001 ms counts nothing, the next interval counts again), W1 and W2 (6 L/min for 60 s gives 6.0 L through print_f88() and through the PS=1 summary).
- AC#2 cases: D1 (row fields as on 1.x), D4 and D6 (no state before a sample), D7 (state after a sample, retain=0).
- Reset cases: X1 to X7 and Q1 to Q5. Q5 is new: a retained reset leaves the total, the file and the published state unchanged.
- OLD (HEAD sources) fails W1, W2, W4, W5, W6 and R1, and has none of the wiring. Every mutant fails its target case, including MR23 (the retained guard removed fails Q5), and the wiring check flips when the properties.retain hand-off is removed.
- An independent slice audit checked 140 generated parts: each is verbatim in its source file (working tree or git HEAD) and brace-complete.
- Not validated on the bench: the OTGW32 is in its WiFi provisioning portal.

Open for the maintainer (does not block this task)
- Announce timing (point 3): keep the boot announce of ADR-176 Must #7, or follow 1.x's just-in-time announce?
- PS=1 residual (harness case R1): the PIC summary repeats its stored MsgID 19 value. If the boiler stops answering MsgID 19 while summaries keep coming, the stale flow keeps counting (80 L in 10 min at a stored 8 L/min). Accept it, or gate PS=1 counting on freshness?
- ADR-176's answered question on partial regression has a stale premise (point 1). A superseding or amending record is the maintainer's call.

Basis of the offline sentence in point 2: the client connects with a clean session and subscribes at QoS 0 (MQTTstuff.ino setCleanSession(true) and subscribe(topic, 0)), so a broker queues nothing for it while it is offline; the only later delivery of that reset is the retained copy, which is ignored.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Cumulative DHW water total for the Home Assistant Energy dashboard (ADR-176), shipped in 2.0.0-alpha.393 (commit 4bbceed61).

What changed
- New dhwWaterMeter.ino: MsgID 19 flow integrated over time with the 1.x rule and its 60000 ms interval cap, persisted in /dhw_water.json (10 L or 15 min, flushed on an orderly restart).
- Write sites in print_f88() (boiler Read-Ack frames) and updatePSSummaryFloatState() (PIC PS=1 summary). Gateway-built answers do not count.
- Discovery row for faux id 241 (dhw_water_total), announced at boot. The state is published by the 60 s task only after a MsgID 19 sample on this boot, and never retained.
- Reset through POST /api/v2/otgw/reset_water_total and MQTT set/<node>/otgw/reset_water_total, applied by loop() to RAM and file together. A reset that arrives as a retained MQTT message is ignored.
- Docs: MQTT.md, openapi.yaml, API README, MANUAL, the EN and NL API chapters, C4 code and component docs, CHANGELOG.

Evidence per AC (all collected on the main tree this session)
- AC#1: 1.x read at origin/otgw-1.x.x 8ba6f7ef9: dhwWaterMeter.ino:38 is DHW_METER_MAX_GAP_MS = 60000UL, and :67 adds flow * dtMs / 60000. This line uses the same constant, the same ordering (seed, move last before the gap test) and flow * dt / 60000. Harness cases U1 (time-based), U2 (60000 ms counts, 60001 ms does not) and W1/W2 (6 L/min for 60 s gives 6.0 L through both write sites) pass.
- AC#2: the discovery row matches 1.x mqtt_configuratie.cpp:66, :653 and :1130 field for field (label, name, water, L, total_increasing, icon, entity_category, enabled). Harness D1 (row), D4/D6 (no state before a sample) and D7 (state after a sample, retain=0) pass.
- AC#3: the divergences from 1.x (persistence, reset surface, announce timing, samples counted, flow bound, identity, arithmetic) are restated in this task's notes. ADR-176 is not edited.
- test/host/test_dhw_water_meter.py --old-rev HEAD: RESULT PASS, 117 checks yes and 0 NO, before and again after the version bump. OLD fails W1, W2, W4, W5, W6 and R1. Every mutant fails its target case, including MR23 for the retained guard.
- The slice audit found all 140 sliced parts verbatim in their sources and brace-complete.
- Host suite on the main tree: 32/34. adr governance is the known TASK-1183 item. heap soak driver unit is a pre-existing timing flake (6/8 on HEAD sources, 7/8 with this change), filed as TASK-1186.
- python evaluate.py --quick: exit 0, health score 100%.
- build.bat --target all after the commit: firmware and filesystem SUCCESS for esp32, esp32-classic and esp32-combo, 3 images created, fresh 2.0.0-alpha.393+4bbceed artifacts; flash use 79.5%, 77.2% and 81.4%.

Not done here
- Bench validation: the OTGW32 sits in its WiFi provisioning portal.
- Maintainer questions, listed in the notes: boot versus just-in-time announce, the PS=1 stale-repeat residual (R1, the one path that can over-count), and the stale premise in ADR-176's partial-regression answer.
<!-- SECTION:FINAL_SUMMARY:END -->
