---
id: TASK-1042
title: >-
  Add -QuietDebugToggles: actively silence telnet debug toggles for leak
  measurement
status: In Progress
assignee:
  - '@claude'
created_date: '2026-07-19 21:20'
updated_date: '2026-07-19 21:26'
labels: []
dependencies: []
priority: medium
ordinal: 161000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Follow-up on TASK-1041. The -SkipDebugToggles switch only guarantees that the capture script does not turn toggles ON; it leaves whatever state the device already had. Field run 2026-07-19 (martreides) showed the failure mode: the device still had OTmsg, REST API, MQTT, MQTTGate, Sensors and NTP all on, left over from a browser-driven capture earlier that same day which switches them all on. The heap-leak preset therefore produced a fully verbose capture despite the switch, which is exactly the instrument perturbation the preset exists to avoid.

Needed: an option that actively turns the toggles off for the duration and restores the original state on exit, so a leak measurement is quiet regardless of what the previous session left behind. Keep -SkipDebugToggles as the 'do not touch' semantics and document the difference.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A switch exists that turns the listed debug toggles off at connect when they are on
- [ ] #2 Original toggle state is restored when the capture ends, including on Ctrl+C
- [ ] #3 capture-heap-leak.bat uses the silencing option instead of the leave-as-is option
- [ ] #4 Run summary records which of the two policies was applied
- [ ] #5 Verified on a device that starts with all toggles on: the resulting telnet log contains only periodic heap stats and no per-message OT or MQTT gate output
<!-- AC:END -->
