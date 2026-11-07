---
id: TASK-1677
title: >-
  Capture tool: read the MQTT broker settings from the gateway instead of typing
  them
status: Done
assignee:
  - '@claude'
created_date: '2026-11-06T21:00:00Z'
updated_date: '2026-11-06T21:10:00Z'
labels:
  - tooling
  - capture
dependencies: []
priority: high
ordinal: 800000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-11-06: a 24 h field capture (maarten_41762) had an empty MQTT stream because the broker IP was typed as 192.168.1.101 while the gateway publishes to 192.168.2.101. The capture tool should read broker host, port and user from the firmware so the tester makes no input error.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 otgw_capture.py reads the MQTT broker host, port and user name from the gateway (REST settings) and uses them as defaults; a typed value that differs from the gateway's is reported before the capture starts
- [x] #2 Works when the gateway is unreachable or has a password: falls back to the saved/prompted value with a clear message
- [x] #3 Host test: red on the old code (typed broker wins silently), green on the fix, using a fake settings response
- [x] #4 Verified against a real bench unit: the tool prints the broker it read from the gateway
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implementation: read_gateway_mqtt() in scripts/otgw_capture.py reads mqttenable, mqttbroker, mqttbrokerport and mqttuser from GET /api/v2/settings (the firmware never returns the password). resolve_inputs(): without --broker the gateway's broker, port and user are used without a prompt and printed; an explicit --broker that differs is kept and reported with a WARNING; MQTT off or no broker on the gateway skips the stream with a message; 401 (admin password), no answer or unreadable answer falls back to the old prompt with a note. --broker-port default is now None (gateway port, else 1883).

Evidence: tests/test_capture_tool.py GatewayBrokerCase (4 tests, fake /api/v2/settings server). Old code: 4 failed (broker '' instead of 192.168.2.101; no differs/off/could-not-read message). Fix: 4 passed. Full capture suites (test_capture_tool, test_capture_launcher, test_capture_otframes): 63 passed, 1 skipped; a first full run had 2 timing failures in FrameGapCase / CaptureThreadTest that passed alone (3 passed) and in the rerun. Bench: read_gateway_mqtt on .63 and .61 returned 192.168.88.32:1883 anonymous; a 25 s real capture on .63 with --no-prompt printed 'MQTT broker read from the gateway: 192.168.88.32:1883, anonymous' and wrote 1225 MQTT messages. evaluate.py --quick: 85 passed, 0 warnings, 0 failed.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
AC1-AC4 done. The capture tool takes the MQTT broker host, port and user from the gateway (GET /api/v2/settings) and warns when a typed broker differs, so a typo can no longer leave the MQTT stream empty (the cause of maarten_41762's empty MQTT capture). Red/green host tests (4/4), full capture suites 63 passed, bench capture on .63 got 1225 MQTT messages via the gateway's broker, evaluator 0 FAIL.
<!-- SECTION:FINAL_SUMMARY:END -->
