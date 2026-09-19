---
id: TASK-1139
title: 'Investigate: gateway reports no thermostat and no boiler after 1.7.5 (GH #684)'
status: To Do
assignee: []
created_date: '2026-09-19 05:09'
updated_date: '2026-09-19 14:23'
labels:
  - bug
  - needs-info
dependencies: []
references:
  - 'https://github.com/rvdbreemen/OTGW-firmware/issues/684'
priority: medium
ordinal: 220000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by Appiejs on GH #684, 2026-09-18. After upgrading to 1.7.5 the gateway shows an empty home screen and reports Thermostat Connected false, Boiler Connected false, OpenTherm Active false, Gateway Mode Monitor. PIC is gateway v6.6 on pic16f1847.

Two things in the report argue AGAINST a 1.7.5 regression, and they should be checked before anyone looks at firmware:

1. He states he also tried 1.7.1 with PIC 6.6 and got the same result. If that holds, whatever changed is not in the firmware.

2. The telnet banner he pasted reads FW 1.7.1+c50cbcc with Up: 0(d)-00:00(H:m), so it was taken within a minute of boot. Boiler OFF and Thermostat OFF that early is expected: no OpenTherm frames have arrived yet, and the 30s liveness window has not even elapsed. The dump may simply be too early to mean anything, which would make the whole report rest on an artefact.

Worth noting what is NOT the problem: the false flags are the firmware correctly reporting no traffic. This is not TASK-1135, where the flags were stale in the other direction.

Gateway Mode Monitor with GW-mode detecting in the banner is the detail most worth chasing. In monitor mode the gateway does not drive the bus, and detecting means the firmware had not yet established the mode at the time of the dump.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 A telnet banner taken at least five minutes after boot is obtained, so the liveness window has elapsed and Boiler/Thermostat OFF means something
- [ ] #2 It is established whether the same symptom really reproduces on 1.7.1, including whether the filesystem was flashed alongside the firmware
- [ ] #3 Either a firmware cause is identified with evidence, or the issue is closed as hardware or wiring with the reasoning recorded for the next reader
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-09-19: asked Appiejs on GH #684 for (a) a telnet banner taken 5+ minutes after boot, since the one he posted reads Up: 0(d)-00:00 and Boiler/Thermostat OFF is expected that early, (b) whether the boiler still runs normally on his thermostat, which separates 'the OTGW sees nothing' from 'there is nothing on the bus', and (c) whether the 1.7.1 test included the filesystem, because firmware-without-filesystem keeps serving the previous web interface and would make 'same result' mean something else. Also flagged Gateway Mode Monitor as worth confirming as deliberate. Pointed at Schelte's hardware checklist as the next step if it still reads empty after five minutes.
<!-- SECTION:NOTES:END -->
