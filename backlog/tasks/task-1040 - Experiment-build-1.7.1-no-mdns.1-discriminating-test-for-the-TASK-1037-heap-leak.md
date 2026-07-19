---
id: TASK-1040
title: >-
  Experiment build 1.7.1-no-mdns.1: discriminating test for the TASK-1037 heap
  leak
status: To Do
assignee: []
created_date: '2026-07-19 15:23'
labels: []
dependencies: []
priority: high
ordinal: 159000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Purpose: settle whether mDNS is the source of the heap leak in TASK-1037, or only the site where the resulting OOM crashes. Waiting for more field captures does not answer this, and the capture script drives a headless browser at 365 REST requests/min, so it measures the fragmentation path rather than the leak.

The experiment: one build, identical to v1.7.1 in every respect except that mDNS is compiled out. Give it to martreides as an alternative to the 1.7.1 he runs now, and read the heap trend.

Both outcomes are informative, which is what makes it worth building:
- Heap stays flat over several hours: mDNS is both leak and crash site. TASK-1037 gets a root cause.
- Heap still decays but no more Exception (2) reboots: mDNS is only the crash site (the ungated unchecked new in _readRRAnswer). We keep a device that survives long enough to instrument, and the leak hunt continues elsewhere.

Critical constraint: base the build on the v1.7.1 tag, NOT on the current 1.7.2-beta.1 dev tip. The single-variable property is the entire value of the experiment; any other delta confounds the result.

Mechanics:
- Compile-time flag (for example OTGW_DISABLE_MDNS) guarding MDNS.begin() and MDNS.addService() in networkStuff.ino:398-406 and the three MDNS.update() call sites (OTGW-firmware.ino:379, OTGW-firmware.ino:427, OTGW-Core.ino:553-554). A build switch, not a code fork.
- Version tag no-mdns.1, giving 1.7.1-no-mdns.1. Valid semver (hyphens are allowed in prerelease identifiers) and self-labelling in the telnet banner, the web UI and the MQTT version topic, so no capture can later be mistaken for a normal 1.7.1 run.
- Set the tag with scripts/autoinc-semver.py --prerelease no-mdns.1. Do NOT use bin/bump-prerelease.sh: its regex is ^[a-zA-Z]+\.[0-9]+$ and rejects the hyphen.
- Experiment branch, never merged to dev, never released. No GitHub release, no Discord beta announcement.

Known side effects to communicate to the tester:
- otgw.local stops resolving. He must use the IP address (192.168.1.150 at last report) for the web UI, and check whether anything in his Home Assistant setup depends on the hostname.
- Because 1.7.1-no-mdns.1 sorts BEFORE 1.7.1 in semver, the firmware update check may offer 1.7.1 as an upgrade and silently end the experiment if he accepts. Verify this behaviour and warn him explicitly.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Build produced from the v1.7.1 tag with mDNS as the only functional difference, verified by diff against the tag
- [ ] #2 Version reports as 1.7.1-no-mdns.1 in the telnet banner, the web UI and the MQTT version topic
- [ ] #3 Firmware and filesystem binaries built and verified to boot, with WiFi, MQTT and the web UI reachable by IP
- [ ] #4 Tester briefed on the otgw.local loss and the update-check downgrade risk before flashing
- [ ] #5 At least 4 hours of heap telemetry collected on the tester device, or a crash captured, whichever comes first
- [ ] #6 Result recorded in TASK-1037 as either 'mDNS is leak and crash site' or 'mDNS is crash site only', with the heap trend as evidence
<!-- AC:END -->
