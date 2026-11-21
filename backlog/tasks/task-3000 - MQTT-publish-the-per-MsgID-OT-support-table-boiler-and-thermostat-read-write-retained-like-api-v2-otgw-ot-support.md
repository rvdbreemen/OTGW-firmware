---
id: TASK-3000
title: >-
  MQTT: publish the per-MsgID OT support table (boiler and thermostat,
  read/write) retained, like /api/v2/otgw/ot-support
status: Done
assignee:
  - '@claude'
created_date: '2026-11-21T10:20:00Z'
updated_date: '2026-11-21T13:55:00Z'
labels:
  - mqtt
  - feature
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 1125000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
gruijter, #alpha-testing 2026-11-21 (msg 1573643085097738272): 'could the per-MsgID support info from GET /api/v2/otgw/ot-support also be published retained on MQTT, for both boiler and thermostat (read/write: ack, unknown, never seen)? Right now MQTT only has otgw-firmware/boiler/unsupported_msgids. My Homey app is MQTT-only, and with the full table it could offer only the controls a setup supports (e.g. SH=/SW=/MW=, TT= via MsgID 9). Something like otgw-firmware/boiler/supported_msgids and otgw-firmware/thermostat/sent_msgids, in the same CSV format.' Robert released and prioritised it (msg 1573643541412839531): 'prioritize this feature request. Publish on MQTT requested information.' Existing: otgw-firmware/boiler/unsupported_msgids, retained CSV (MQTTstuff.ino:2374; TASK-1781 empty-at-connect fix). Check ADR-101 (flat per-value topics) and ADR-088 (burst windowing) for the topic shape; a CSV list per category matches the existing topic. Data source is the same as /api/v2/otgw/ot-support.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Retained MQTT topics publish, for the boiler, the MsgIDs with read ack, write ack and unknown (CSV, same format as unsupported_msgids), and for the thermostat the MsgIDs it sent as read and as write; the values match GET /api/v2/otgw/ot-support on the same unit
- [x] #2 Topics update on change and after an MQTT reconnect without overwriting a retained value with an empty one before the data is known (TASK-1781 lesson)
- [x] #3 Topic names and payloads documented in the MQTT docs; gruijter told the topic names in #alpha-testing
- [x] #4 Host test on the real publish code red before / green after; evaluate --quick no new FAIL; bench check on a unit via the NAS broker
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Two retained CSV topics in the unsupported_msgids shape (ADR-101 holds: a CSV scalar, no JSON; precedent ADR-142/unsupported_msgids): otgw-firmware/boiler/supported_msgids (blAR/blAW) and otgw-firmware/thermostat/sent_msgids (tsR/tsW). unsupported_msgids stays byte-identical (Robert's A on TASK-1781: it may be published empty). Builder publishMsgidCsv() sizes a transient malloc from the bit count (no truncation, no permanent RAM); an empty list is not published (the bitmaps only grow, empty = not known yet, AC2). New MQTT dirty bits otSupportListsDirty (OT_SUPPORT_LIST_BOILER/THERMOSTAT) set where an acked/sent bit is newly set; doTaskMinuteChanged publishes only the changed list and clears only what was published. Connect: own stage 5 in connStageTick (gated on the outbox and the largest block, ADR-088), not appended to stage 4. Tests: new tests/test_mqtt_ot_support_lists.py (real bitmap block + real publisher, red on origin/dev), test_mqtt_connect_staging.py expectations 4->5 portions, test_boiler_support_bitmap_b_only.py prelude. Docs: docs/api/MQTT.md, manuals ch09/h09, c4. Bench: .61 loopback and .63 have empty boiler bitmaps, so the bench-tester uploads format-3 /ot-boiler.json and /ot-thermo.json with long lists (incl. ids >= 100), reboots, and diffs GET /api/v2/otgw/ot-support against the retained topics on the NAS broker.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-21 (92): answered gruijter's follow-up (msg 1573644409688432691 -> 1573644897041391718). Facts from OTGW-Core.ino:6217-6290: the boiler bitmaps take every genuine B answer, whoever sent the request (T from the thermostat or R from the gateway, so SAT's own requests count too); bLocalAnswer and bReplayed frames do not count. The thermostat bitmaps take only real T frames (TASK-1612/1895). The bitmaps are persisted (/ot-boiler.json, /ot-thermo.json), so the acked lists only grow; an unsupported verdict can be retracted by a later genuine ack (TASK-1084).

2026-11-21 (otgw-dev): implemented in lane/d3-otsupport (wt-d3-otsupport). tests/test_mqtt_ot_support_lists.py 10/10 on the fix; on the origin/dev sources (OTGW_CORE_INO/OTGW_MQTT_INO) 3 FAIL (no dirty bits: 0 != 1/2/3) + Publisher ERROR (publishMsgidCsv() not found). test_mqtt_connect_staging.py: red on the stage-5 change as expected ([6,6,15,2,2] vs [6,6,15,2], 34 vs 32 packets, 5 vs 4 portions), expectations updated, 9/9 OK. test_boiler_support_bitmap_b_only.py OK (prelude declares otSupportListsDirty). evaluate --quick 88 passed / 0 failed.

2026-11-21 (otgw-dev): review (subagent, read-only) found 1 risk: a list refused at connect stage 5 (heap, a forced portion under the 16 KB floor, malloc) was never retried. Fixed: stage 5 marks the lists it could not send (markOtSupportListsDirty), the minute tick retries; test_mqtt_connect_staging.py test_support_lists_refused_at_connect_are_marked_for_the_minute_tick red without the mark line (marked=-1) / green. Comments 'four portions' -> five, docs mention the connect republish. Low risk left as is: the minute tick may publish a changed list while stages 1-4 still run (as unsupported_msgids already does). Full sweep 355/357 (test_manual_generated fixed by scripts/build_manual.py; 1 skip); test/host/test_boiler_unsupported_origin.py PASS after slicing the new enum.

2026-11-21 (otgw-dev): topic -> REST mapping (per msgid in GET /api/v2/otgw/ot-support): blAR <=> '<id>R' in otgw-firmware/boiler/supported_msgids, blAW <=> '<id>W' there; tsR <=> '<id>R' in otgw-firmware/thermostat/sent_msgids, tsW <=> '<id>W' there; blUR/blUW <=> '<id>R'/'<id>W' in the existing otgw-firmware/boiler/unsupported_msgids; never seen = in none of the three. Example payloads: boiler/supported_msgids '0R,3R,5R,25R,125R,1W,16W,56W', thermostat/sent_msgids '0R,3R,17R,1W,16W,24W' (read entries first, then write, each ascending). Two topics, not four: the R/W suffix keeps the direction, it is the format of unsupported_msgids that gruijter asked to match, and one parser serves all three topics (ADR-101 holds: a string scalar per topic, no JSON).

2026-11-21 12:22 (otgw-dev): rebased onto bce9496a5 (TASK-2993 push), one commit fd70da50c, bump beta.411. Build esp32-combo 12:11-12:22 exit 0, 'Build completed successfully': 2.0.0-beta.411+fd70da5; ino.bin 2154528 B vs 2153920 B on bce9496 = +608 B; app partition 2154091/2293760 B (93.9%). Artifacts + SHA256SUMS: OTGW-artifacts\beta.411+fd70da5\. Bench files (format-3 ot-boiler.json / ot-thermo.json with long lists + expected-topics.txt): OTGW-artifacts\task3000-bench\. Bench requested via the coordinator. Not yet: bench (AC1/AC2 on device, AC4 bench part), gruijter notice (AC3, after the push).

Bench (otgw-bench-tester, 2026-11-21, .61 OTGW32 loopback, beta.411+fd70da5 app 8e2c2dfe/fs 402d08af, NAS broker 192.168.88.36): AC1 bench: after upload of task3000-bench ot-boiler.json/ot-thermo.json + reboot 14:08:04, boiler/supported_msgids, thermostat/sent_msgids, boiler/unsupported_msgids each byte-equal to expected-topics.txt (1 msg each, 0 empty); GET /api/v2/otgw/ot-support blAR/blAW, tsR/tsW, blUR/blUW lists == expected. AC2 bench (reconnect part): MQTT drop via mqttbroker 192.0.2.1 14:13:08 -> back 14:13:49, after the reconnect 1 msg per topic, 0 not-equal, 0 empty; no empty value during the reboot either (sub without -R from before the reboot). On-change part: host test only (no external trigger on loopback). AC4 bench: done via the NAS broker as above. Files + settings restored, settings.ini no diff. Evidence: OTGW-field-data/bench-otgw32/2026-11-21_task3000-fd70da5/ (steps.log, live-sub.raw, live-msgids.txt, step4-msgids.txt, ot-support.json, SHA256SUMS).

2026-11-21 (otgw-dev): AC3: docs in docs/api/MQTT.md, manuals ch09/h09 (+ MANUAL.md), c4; gruijter told the topic names, example payloads and the rules by BlueFlame in #alpha-testing (msg 1573690224960344067, read back equal).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1: retained otgw-firmware/boiler/supported_msgids (blAR/blAW) and otgw-firmware/thermostat/sent_msgids (tsR/tsW) next to the existing unsupported_msgids (blUR/blUW), CSV '<id>R...,<id>W...'; bench .61 (beta.411+fd70da5, uploaded long-list ot-*.json): all three byte-equal to expected-topics.txt and equal to GET /api/v2/otgw/ot-support (otgw-bench-tester, OTGW-field-data/bench-otgw32/2026-11-21_task3000-fd70da5/). AC2: update per minute on a newly set bit (OT_SUPPORT_LIST_* dirty bits), republish in its own connect stage 5 with a retry mark on refusal, never published empty; host tests tests/test_mqtt_ot_support_lists.py 10/10 (red on origin/dev: 3 FAIL + ERROR) and test_mqtt_connect_staging.py 10/10 (retry test red without the mark); bench: 0 empty values during reboot and after an MQTT drop/reconnect. AC3: docs/api/MQTT.md, manuals EN/NL, MANUAL.md, c4; gruijter told in #alpha-testing msg 1573690224960344067. AC4: host tests red/green as above, full sweep 355/357 (manual regen fixed), evaluate --quick 0 FAIL, bench via the NAS broker. Pushed 8c929eee2 (beta.411), zip post 1573690022371135490; flash +608 B (app partition 93.9 %). Not verified on a device: the per-minute update after a live new MsgID (no external trigger on a loopback unit; host test only).
<!-- SECTION:FINAL_SUMMARY:END -->
