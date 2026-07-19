---
id: TASK-1041
title: >-
  Add capture-heap-leak.bat: browser-free capture preset for the TASK-1037 leak
  measurement
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 15:26'
updated_date: '2026-07-19 15:27'
labels: []
dependencies: []
priority: high
ordinal: 160000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The existing capture-mqtt-debug.bat cannot measure the TASK-1037 leak, because it drives a headless Edge against the web UI. In the martreides 1.6.1 capture that produced 365 REST requests/min sustained (13067 hits each on /api/v2/device/time and /api/v2/otgw/otmonitor), plus a crashlog poll every 30s that costs ~1021 ms and visibly drops free heap by ~3.4 KB per poll. That run therefore shows the HTTP fragmentation profile, not the leak ramp: maxBlock pinned at 5352 while free heap held 10-13 KB.

To see the leak we need the device idle apart from its normal MQTT and OpenTherm work, with telnet as the only extra connection.

The underlying script already has the switches needed (-SkipBrowserCapture, -CrashlogPollSeconds, -OutputRoot) and forwards all arguments verbatim from the .bat launcher. So this is a thin preset wrapper, not a second implementation: no duplication of the 2213-line worker, and it inherits every future fix to it.

Preset:
- -SkipBrowserCapture, the whole point.
- -CrashlogPollSeconds 3600 rather than -SkipCrashlogCapture, so a crash is still captured within the hour without the 30s poll perturbing the measurement.
- -OutputRoot logs/heap-leak so these runs do not mix with normal diagnostic captures.
- User arguments forwarded after the preset so -DeviceHost and friends still work and any preset value can be overridden.

The wrapper must also tell the operator what invalidates the run, since that is the failure mode that already cost us one capture: no OTGW web UI open in any browser tab for the duration.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 scripts/capture-heap-leak.bat exists and forwards user arguments to capture-mqtt-debug.bat after the preset flags
- [ ] #2 A run produces telnet and MQTT logs with zero REST requests from the capture tooling in the telnet log
- [ ] #3 Output lands under logs/heap-leak, separate from normal diagnostic captures
- [ ] #4 The script prints, before starting, that the web UI must stay closed for the duration
- [ ] #5 Referenced from TASK-1040 as the capture method for the no-mdns experiment
<!-- AC:END -->
